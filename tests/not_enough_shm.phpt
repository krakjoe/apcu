--TEST--
Error if cache structures cannot be allocated in SHM
--INI--
apc.enabled=1
apc.enable_cli=1
apc.shm_size=1M
apc.entries_hint=1000000
--FILE--
Irrelevant
--EXPECTF--
%A: Unable to allocate %d bytes of shared memory for cache structures. Either apc.shm_size is too small or apc.entries_hint too large in Unknown on line 0
