--TEST--
Stress test: Run APCu with a huge amount of simultaneous apcu_add() and apcu_store() operations
--SKIPIF--
<?php
require_once(__DIR__ . '/skipif.inc');
if (substr(PHP_OS, 0, 3) === 'WIN') die('skip Windows not supported');
if (!getenv('RUN_APCU_RESOURCE_HEAVY_TESTS')) die('skip APCu resource heavy tests not enabled (to enable set "export RUN_APCU_RESOURCE_HEAVY_TESTS=1")');
?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.shm_size=1M
--FILE--
<?php

// This test stresses APCu with simultaneous apcu_add() and apcu_store() operations done by multiple processes.
// Goal of this test is to detect hangs / crashes as locking problems or memory access violations which can only
// occur during concurrent usage. The more iterations and/or parallel use of CPU cores, the higher the probability
// that this test can find a rarely occurring problem. To prevent the CI pipeline from running unnecessarily long
// in case of locking problems or stuck processes, a watchdog process is started in parallel with the test processes.
// If the watchdog times out or if one of the other processes terminates abnormally, then all processes are
// immediately terminated using SIGKILL, as this is the only safe way to terminate hanging processes in some cases.

function get_cpu_cores(): int {
    $cpu_cores = (int) exec('nproc');
    if ($cpu_cores <= 0) {
        echo "Error executing nproc to determine the number of CPU cores.\n";
        exit(1);
    }

    return $cpu_cores;
}

function stress_test(int $iterations) {
    $apcu_store_failures = 0;

    for ($i = 0; $i < $iterations; $i++) {
        $keyLen = random_int(1, 2);
        $valLen = random_int(1, 500);

        // choose by random if existing entries are replaced
        if ($valLen & 1) {
            apcu_add(random_bytes($keyLen), random_bytes($valLen));
        } else {
            if (!apcu_store(random_bytes($keyLen), random_bytes($valLen))) {
                // should only happen very rarely under extreme load
                $apcu_store_failures++;
                if ($apcu_store_failures > 10) {
                    echo "Too many apcu_store() failures!\n";
                    exit(1);
                }
            }
        }
    }
}


$watchdog_timeout_seconds = 60;
$iterations = 250000;
$num_processes = get_cpu_cores();

if ($num_processes < 2) {
    echo "Too few CPU cores to run this test.\n";
    exit(1);
}

// start watchdog process
$watchdog_pid = pcntl_fork();
if ($watchdog_pid === 0) {
    // child process (watchdog)
    sleep($watchdog_timeout_seconds);
    exit(1);
}

$pid_list = [];
$pid_list[$watchdog_pid] = $watchdog_pid;

echo "Starting stress test (processes / iterations): $num_processes / $iterations\n";

// fork child processes
for ($i = 0; $i < $num_processes; $i++) {
    $pid = pcntl_fork();
    if ($pid === 0) {
        // child process (generates APCu load)
        stress_test($iterations);
        exit(0);
    }

    $pid_list[$pid] = $pid;
}

// the parent process waits for all child processes to finish
while (count($pid_list) > 1) {
    $pid = pcntl_wait($status);
    unset($pid_list[$pid]);

    // check state / return code of the finished child process
    if (!pcntl_wifexited($status) || pcntl_wexitstatus($status) !== 0) {
        if ($pid === $watchdog_pid) {
            echo "Watchdog process timed out after {$watchdog_timeout_seconds} seconds.\n";
        } else {
            echo "Child process with PID $pid exited abnormally.\n";
        }

        echo "This should be investigated!\n";
        echo "Killing all remaining child processes with SIGKILL...\n";
        exec('kill -9 ' . implode(' ', $pid_list));
        exit(1);
    }
}

// if all test processes finished normally, the watchdog process is still running and must be aborted
if (isset($pid_list[$watchdog_pid])) {
    exec('kill ' . $watchdog_pid);
}

echo "Finished stress test successfully.\n";

?>
--EXPECTF--
Starting stress test (processes / iterations): %d / %d
Finished stress test successfully.
