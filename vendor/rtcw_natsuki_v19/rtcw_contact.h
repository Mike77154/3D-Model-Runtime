#ifndef RTCW_CONTACT_H
#define RTCW_CONTACT_H
/* Contact sheets from validated P6 800x800 snapshots, 4x3 thumbs. */
int rtcw_contact_generate(const char *folder,unsigned int shots);
const char *rtcw_contact_error(void);
#endif
