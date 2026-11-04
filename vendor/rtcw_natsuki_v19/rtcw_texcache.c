/* CC0, C89. Fixed-size LRU cache; no heap and no point floating. */
#include "rtcw_texcache.h"
#include <string.h>
#include <limits.h>
#define RTCW_CACHE_PATH 768U
typedef struct cached_s {
    char path[RTCW_CACHE_PATH];
    rtcw_texture texture;
    unsigned long age;
    unsigned int valid;
} cached;
static cached entries[RTCW_TEXCACHE_SLOTS];
static unsigned long tick,hits,misses,evictions;
void rtcw_texcache_reset(void){
    unsigned int i;
    for(i=0U;i<RTCW_TEXCACHE_SLOTS;i++){entries[i].valid=0U;entries[i].path[0]=0;entries[i].age=0UL;}
    tick=0UL;hits=0UL;misses=0UL;evictions=0UL;
}
void rtcw_texcache_invalidate(const char *filename){
    unsigned int i;
    if(!filename)return;
    for(i=0U;i<RTCW_TEXCACHE_SLOTS;i++){
        if(entries[i].valid&&strcmp(entries[i].path,filename)==0){
            entries[i].valid=0U;entries[i].path[0]=0;
        }
    }
}
unsigned long rtcw_texcache_hits(void){return hits;}
unsigned long rtcw_texcache_misses(void){return misses;}
unsigned long rtcw_texcache_evictions(void){return evictions;}
static void expire_on_counter_wrap(void){
    unsigned int i;
    for(i=0U;i<RTCW_TEXCACHE_SLOTS;i++)entries[i].valid=0U;
    tick=0UL;
}
const rtcw_texture *rtcw_texcache_get(const char *filename){
    unsigned int i,which=RTCW_TEXCACHE_SLOTS;
    unsigned long oldest=ULONG_MAX;
    if(!filename||strlen(filename)>=RTCW_CACHE_PATH||!filename[0])return NULL;
    if(tick==ULONG_MAX)expire_on_counter_wrap();
    for(i=0U;i<RTCW_TEXCACHE_SLOTS;i++){
        if(entries[i].valid&&strcmp(entries[i].path,filename)==0){
            hits++;entries[i].age=++tick;return &entries[i].texture;
        }
    }
    misses++;
    if(!rtcw_texture_load(filename))return NULL;
    for(i=0U;i<RTCW_TEXCACHE_SLOTS;i++){
        if(!entries[i].valid){which=i;break;}
        if(entries[i].age<oldest){oldest=entries[i].age;which=i;}
    }
    if(which==RTCW_TEXCACHE_SLOTS)return NULL;
    if(entries[which].valid)evictions++;
    strcpy(entries[which].path,filename);
    entries[which].texture=rtcw_texture_current;
    entries[which].valid=1U;entries[which].age=++tick;
    return &entries[which].texture;
}
