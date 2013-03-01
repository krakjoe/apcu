/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2011 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Rasmus Lerdorf <rasmus@php.net>                             |
  +----------------------------------------------------------------------+

   This software was contributed to PHP by Community Connect Inc. in 2002
   and revised in 2005 by Yahoo! Inc. to add support for PHP 5.1.
   Future revisions and derivatives of this source code must acknowledge
   Community Connect Inc. as the original contributor of this module by
   leaving this note intact in the source code.

   All other licensing and usage conditions are those of the PHP Group.

 */

/* $Id: apc_rfc1867.c 309694 2011-03-25 18:47:38Z rasmus $*/

#include "apc.h"
#include "apc_globals.h"
#include "rfc1867.h"

#ifdef PHP_WIN32
#include "win32/time.h"
#endif

#ifdef MULTIPART_EVENT_FORMDATA
extern int _apc_store(char *strkey, int strkey_len, const zval *val, const uint ttl, const int exclusive TSRMLS_DC);
extern int _apc_update(char *strkey, int strkey_len, apc_cache_updater_t updater, void* data TSRMLS_DC);

static int update_bytes_processed(apc_cache_t* cache, apc_cache_entry_t* entry, void* data) {
    int *bytes_ptr = (int*)data;
    zval* val = entry->data.user.val;

    if(Z_TYPE_P(val) == IS_ARRAY) {
        HashTable *ht = val->value.ht;
        Bucket* curr = NULL;
        for (curr = ht->pListHead; curr != NULL; curr = curr->pListNext) {
            if(curr->nKeyLength == 8 && 
                (!memcmp(curr->arKey, "current", curr->nKeyLength))) {
                zval* current =  ((zval**)curr->pData)[0];
                current->value.lval = *bytes_ptr;
                return 1;
            }
        }
    }

    return 0;
}

static double my_time() {
    struct timeval a;
    double t;
    gettimeofday(&a, NULL);
    t = a.tv_sec + (a.tv_usec/1000000.00);
    return t;
}


#define RFC1867_DATA(name) \
                ((request_data)->name)

int apc_rfc1867_progress(uint event, void *event_data, void **extra TSRMLS_DC) {
    apc_rfc1867_data *request_data = &APCG(rfc1867_data);
    zval *track = NULL;

    switch (event) {
        case MULTIPART_EVENT_START:
            {
                multipart_event_start *data = (multipart_event_start *) event_data;
                
                RFC1867_DATA(content_length)    = data->content_length;
                RFC1867_DATA(tracking_key)[0]   = '\0';
                RFC1867_DATA(name)[0]           = '\0';
                RFC1867_DATA(cancel_upload)     = 0;
                RFC1867_DATA(temp_filename)     = NULL;
                RFC1867_DATA(filename)[0]       = '\0';
                RFC1867_DATA(key_length)        = 0;
                RFC1867_DATA(start_time)        = my_time();
                RFC1867_DATA(bytes_processed)   = 0;
                RFC1867_DATA(prev_bytes_processed) = 0;
                RFC1867_DATA(rate)              = 0;
                RFC1867_DATA(update_freq)       = (int) APCG(rfc1867_freq);
                RFC1867_DATA(started)           = 0;
                
                if(RFC1867_DATA(update_freq) < 0) {  // frequency is a percentage, not bytes
                    RFC1867_DATA(update_freq) = (int) (RFC1867_DATA(content_length) * APCG(rfc1867_freq) / 100); 
                }
            }
            break;

        case MULTIPART_EVENT_FORMDATA:
            {
                int prefix_len = strlen(APCG(rfc1867_prefix));
                multipart_event_formdata *data = (multipart_event_formdata *) event_data;
                if(data->name && !strncasecmp(data->name, APCG(rfc1867_name), strlen(APCG(rfc1867_name))) 
                    && data->value && data->length) { 
                    
                    if(data->length >= sizeof(RFC1867_DATA(tracking_key)) - prefix_len) {
                        apc_warning("Key too long for '%s'. Maximum size is '%d' characters." TSRMLS_CC, 
                                    APCG(rfc1867_name), 
                                    sizeof(RFC1867_DATA(tracking_key)) - prefix_len);
                        break;
                    }

                    if(RFC1867_DATA(started)) {
                        apc_warning("Upload progress key '%s' should be before the file upload entry in the form." TSRMLS_CC, 
                                    APCG(rfc1867_name)); 
                        break;
                    }

                    strlcat(RFC1867_DATA(tracking_key), APCG(rfc1867_prefix), 63);
                    strlcat(RFC1867_DATA(tracking_key), *data->value, 63);
                    RFC1867_DATA(key_length) = data->length + prefix_len;
                    RFC1867_DATA(bytes_processed) = data->post_bytes_processed;
                }
            }
            break;

        case MULTIPART_EVENT_FILE_START:
            {
                RFC1867_DATA(started) = 1;
                if(*RFC1867_DATA(tracking_key)) {
                    multipart_event_file_start *data = (multipart_event_file_start *) event_data;

                    RFC1867_DATA(bytes_processed) = data->post_bytes_processed;
                    strlcpy(RFC1867_DATA(filename),*data->filename,128);
                    RFC1867_DATA(temp_filename) = NULL;
                    strlcpy(RFC1867_DATA(name),data->name,64);
                    ALLOC_INIT_ZVAL(track);
                    array_init(track);
                    add_assoc_long(track, "total", RFC1867_DATA(content_length));
                    add_assoc_long(track, "current", RFC1867_DATA(bytes_processed));
                    add_assoc_string(track, "filename", RFC1867_DATA(filename), 1);
                    add_assoc_string(track, "name", RFC1867_DATA(name), 1);
                    add_assoc_long(track, "done", 0);
                    add_assoc_double(track, "start_time", RFC1867_DATA(start_time));
                    _apc_store(RFC1867_DATA(tracking_key), RFC1867_DATA(key_length)+1, track, APCG(rfc1867_ttl), 0 TSRMLS_CC);
                    zval_ptr_dtor(&track);
                }
            }
            break;

        case MULTIPART_EVENT_FILE_DATA:
            if(*RFC1867_DATA(tracking_key)) {
                multipart_event_file_data *data = (multipart_event_file_data *) event_data;
                RFC1867_DATA(bytes_processed) = data->post_bytes_processed;
                if(RFC1867_DATA(bytes_processed) - RFC1867_DATA(prev_bytes_processed) > (uint) RFC1867_DATA(update_freq)) {
                    if(!_apc_update(RFC1867_DATA(tracking_key), RFC1867_DATA(key_length), update_bytes_processed, &RFC1867_DATA(bytes_processed) TSRMLS_CC)) {
                        ALLOC_INIT_ZVAL(track);
                        array_init(track);
                        add_assoc_long(track, "total", RFC1867_DATA(content_length));
                        add_assoc_long(track, "current", RFC1867_DATA(bytes_processed));
                        add_assoc_string(track, "filename", RFC1867_DATA(filename), 1);
                        add_assoc_string(track, "name", RFC1867_DATA(name), 1);
                        add_assoc_long(track, "done", 0);
                        add_assoc_double(track, "start_time", RFC1867_DATA(start_time));
                        _apc_store(RFC1867_DATA(tracking_key), RFC1867_DATA(key_length)+1, track, APCG(rfc1867_ttl), 0 TSRMLS_CC);
                        zval_ptr_dtor(&track);
                    }
                    RFC1867_DATA(prev_bytes_processed) = RFC1867_DATA(bytes_processed);
                }
            }
            break;

        case MULTIPART_EVENT_FILE_END:
            if(*RFC1867_DATA(tracking_key)) {
                multipart_event_file_end *data = (multipart_event_file_end *) event_data;
                RFC1867_DATA(bytes_processed) = data->post_bytes_processed;
                RFC1867_DATA(cancel_upload) = data->cancel_upload;
                if(data->temp_filename) {
                    RFC1867_DATA(temp_filename) = data->temp_filename;
                } else {
                    RFC1867_DATA(temp_filename) = "";
                }
                ALLOC_INIT_ZVAL(track);
                array_init(track);
                add_assoc_long(track, "total", RFC1867_DATA(content_length));
                add_assoc_long(track, "current", RFC1867_DATA(bytes_processed));
                add_assoc_string(track, "filename", RFC1867_DATA(filename), 1);
                add_assoc_string(track, "name", RFC1867_DATA(name), 1);
                add_assoc_string(track, "temp_filename", RFC1867_DATA(temp_filename), 1);
                add_assoc_long(track, "cancel_upload", RFC1867_DATA(cancel_upload));
                add_assoc_long(track, "done", 0);
                add_assoc_double(track, "start_time", RFC1867_DATA(start_time));
                _apc_store(RFC1867_DATA(tracking_key), RFC1867_DATA(key_length)+1, track, APCG(rfc1867_ttl), 0 TSRMLS_CC);
                zval_ptr_dtor(&track);
            }
            break;

        case MULTIPART_EVENT_END:
            if(*RFC1867_DATA(tracking_key)) {
                double now = my_time(); 
                multipart_event_end *data = (multipart_event_end *) event_data;
                RFC1867_DATA(bytes_processed) = data->post_bytes_processed;
                if(now>RFC1867_DATA(start_time)) RFC1867_DATA(rate) = 8.0*RFC1867_DATA(bytes_processed)/(now-RFC1867_DATA(start_time));
                else RFC1867_DATA(rate) = 8.0*RFC1867_DATA(bytes_processed);  /* Too quick */
                ALLOC_INIT_ZVAL(track);
                array_init(track);
                add_assoc_long(track, "total", RFC1867_DATA(content_length));
                add_assoc_long(track, "current", RFC1867_DATA(bytes_processed));
                add_assoc_double(track, "rate", RFC1867_DATA(rate));
                add_assoc_string(track, "filename", RFC1867_DATA(filename), 1);
                add_assoc_string(track, "name", RFC1867_DATA(name), 1);
                add_assoc_long(track, "cancel_upload", RFC1867_DATA(cancel_upload));
                add_assoc_long(track, "done", 1);
                add_assoc_double(track, "start_time", RFC1867_DATA(start_time));
                _apc_store(RFC1867_DATA(tracking_key), RFC1867_DATA(key_length)+1, track, APCG(rfc1867_ttl), 0 TSRMLS_CC);
                zval_ptr_dtor(&track);
            }
            break;
    }

    return SUCCESS;
}

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim>600: expandtab sw=4 ts=4 sts=4 fdm=marker
 * vim<600: expandtab sw=4 ts=4 sts=4
 */
