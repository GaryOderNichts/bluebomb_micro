#!/bin/bash
# TODO The github workflow currently uses this to build all binaries, this could be moved to cmake machinery too
set -e
set -o pipefail

targets=("MINI_SM_NTSC" "MINI_SM_PAL" "WII_SM2_0E" "WII_SM2_0J" "WII_SM2_0U" "WII_SM2_1E" "WII_SM2_2E" "WII_SM2_2J" "WII_SM2_2U" "WII_SM3_0E" "WII_SM3_0J" "WII_SM3_0U" "WII_SM3_1E" "WII_SM3_1J" "WII_SM3_1U" "WII_SM3_2E" "WII_SM3_2J" "WII_SM3_2U" "WII_SM3_3E" "WII_SM3_3J" "WII_SM3_3U" "WII_SM3_4E" "WII_SM3_4J" "WII_SM3_4U" "WII_SM3_5K" "WII_SM4_0E" "WII_SM4_0J" "WII_SM4_0U" "WII_SM4_1E" "WII_SM4_1J" "WII_SM4_1K" "WII_SM4_1U" "WII_SM4_2E" "WII_SM4_2J" "WII_SM4_2K" "WII_SM4_2U" "WII_SM4_3E" "WII_SM4_3J" "WII_SM4_3K" "WII_SM4_3U")
for target in ${targets[@]}; do
    cmake -H. -Bbuild -DBLUEBOMB_TARGET="$target"
    cmake --build build -j4
done
