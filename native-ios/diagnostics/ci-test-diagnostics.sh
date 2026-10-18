#!/bin/bash
set -euo pipefail
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/diagnostics/test_recovery.c -o native-build/test-recovery
native-build/test-recovery | tee native-build/evidence/native-recovery-test.txt
mkdir -p native-build/evidence
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fobjc-arc -DNATIVE_JOURNAL_TEST -fsanitize=address,undefined native-ios/diagnostics/native_journal.m native-ios/diagnostics/test_journal.m -framework Foundation -o native-build/test-journal
native-build/test-journal | tee native-build/evidence/native-journal-test.txt
clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fobjc-arc -DNATIVE_JOURNAL_TEST -fsanitize=thread native-ios/diagnostics/native_journal.m native-ios/diagnostics/test_journal.m -framework Foundation -o native-build/test-journal-thread
native-build/test-journal-thread | tee native-build/evidence/native-journal-thread-test.txt

clang -std=gnu11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined native-ios/diagnostics/native_profile.c native-ios/diagnostics/test_profile.c -o native-build/test-profile
native-build/test-profile | tee native-build/evidence/native-profile-test.txt
