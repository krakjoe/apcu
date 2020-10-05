<?php

/**
 * @generate-function-entries
 * @generate-legacy-arginfo
 */

class APCUIterator implements Iterator {
    /** @param array|string|null $search */
    public function __construct(
        $search = null,
        int $format = APC_ITER_ALL,
        int $chunk_size = 0,
        int $list = APC_LIST_ACTIVE);

    /** @return void */
    public function rewind();

    /** @return void */
    public function next();

    /** @return bool */
    public function valid();

    /** @return string|int|false */
    public function key();

    /** @return mixed */
    public function current();

    /** @return int */
    public function getTotalHits();

    /** @return int */
    public function getTotalSize();

    /** @return int */
    public function getTotalCount();
}
