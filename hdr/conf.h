/**
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 *
 * GitHub Co-pilot and <jens@bennerhq.com> wrote this file.  As long as you 
 * retain this notice you can do whatever you want with this stuff. If we meet 
 * some day, and you think this stuff is worth it, you can buy me a beer in 
 * return.   
 *
 * /benner
 * ----------------------------------------------------------------------------
 */
#ifndef __CONF_H__
#define __CONF_H__

typedef struct {
    char **keys;
    char **values;
    size_t count;
} Config;

Config conf_read_file(const char *filename);
void conf_free(Config *config);
void conf_print(const Config *config);
const char *conf_get(const Config *config, const char *key, const char* default_value);

#endif /* __CONF_H__ */
