#!/bin/bash
set -euo pipefail
mkdir -p native-build/evidence
for sanitizer in address,undefined thread; do
 clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize="$sanitizer" native-ios/threading/test_mailbox_claim.c -lpthread -o native-build/test-mailbox-claim
 native-build/test-mailbox-claim | tee "native-build/evidence/mailbox-claim-$sanitizer.txt"
done
clang -std=gnu11 -Wall -Wextra -Werror -O1 native-ios/threading/test_mailbox_claim.c -lpthread -o native-build/bench-mailbox-claim
native-build/bench-mailbox-claim bench | tee native-build/evidence/mailbox-claim-benchmark.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer native-ios/threading/test_native_session.c -o native-build/test-native-session
native-build/test-native-session | tee native-build/evidence/native-session-test.txt
for sanitizer in address,undefined thread; do
  clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize="$sanitizer" -fno-omit-frame-pointer native-ios/threading/test_session_bridge.c -lpthread -o native-build/test-session-bridge
  native-build/test-session-bridge | tee "native-build/evidence/native-session-bridge-$sanitizer.txt"
done
