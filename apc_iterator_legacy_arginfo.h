/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: e8a5a86d5bb9209117834ed0071130889e769c34 */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_APCUIterator___construct, 0, 0, 0)
	ZEND_ARG_INFO(0, search)
	ZEND_ARG_INFO(0, format)
	ZEND_ARG_INFO(0, chunk_size)
	ZEND_ARG_INFO(0, list)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_APCUIterator_rewind, 0, 0, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_APCUIterator_next arginfo_class_APCUIterator_rewind

#define arginfo_class_APCUIterator_valid arginfo_class_APCUIterator_rewind

#define arginfo_class_APCUIterator_key arginfo_class_APCUIterator_rewind

#define arginfo_class_APCUIterator_current arginfo_class_APCUIterator_rewind

#define arginfo_class_APCUIterator_getTotalHits arginfo_class_APCUIterator_rewind

#define arginfo_class_APCUIterator_getTotalSize arginfo_class_APCUIterator_rewind

#define arginfo_class_APCUIterator_getTotalCount arginfo_class_APCUIterator_rewind


ZEND_METHOD(APCUIterator, __construct);
ZEND_METHOD(APCUIterator, rewind);
ZEND_METHOD(APCUIterator, next);
ZEND_METHOD(APCUIterator, valid);
ZEND_METHOD(APCUIterator, key);
ZEND_METHOD(APCUIterator, current);
ZEND_METHOD(APCUIterator, getTotalHits);
ZEND_METHOD(APCUIterator, getTotalSize);
ZEND_METHOD(APCUIterator, getTotalCount);


static const zend_function_entry class_APCUIterator_methods[] = {
	ZEND_ME(APCUIterator, __construct, arginfo_class_APCUIterator___construct, ZEND_ACC_PUBLIC)
	ZEND_ME(APCUIterator, rewind, arginfo_class_APCUIterator_rewind, ZEND_ACC_PUBLIC)
	ZEND_ME(APCUIterator, next, arginfo_class_APCUIterator_next, ZEND_ACC_PUBLIC)
	ZEND_ME(APCUIterator, valid, arginfo_class_APCUIterator_valid, ZEND_ACC_PUBLIC)
	ZEND_ME(APCUIterator, key, arginfo_class_APCUIterator_key, ZEND_ACC_PUBLIC)
	ZEND_ME(APCUIterator, current, arginfo_class_APCUIterator_current, ZEND_ACC_PUBLIC)
	ZEND_ME(APCUIterator, getTotalHits, arginfo_class_APCUIterator_getTotalHits, ZEND_ACC_PUBLIC)
	ZEND_ME(APCUIterator, getTotalSize, arginfo_class_APCUIterator_getTotalSize, ZEND_ACC_PUBLIC)
	ZEND_ME(APCUIterator, getTotalCount, arginfo_class_APCUIterator_getTotalCount, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};
