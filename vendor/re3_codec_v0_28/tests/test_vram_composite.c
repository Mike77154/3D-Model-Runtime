/* RE3 PLD + PLW VRAM composition regression, C89/no heap. */
#include "re3_packed.h"
#include <stdio.h>
static re3c_tim_vram player,weapon,merged;
int main(void){int rc;re3c_tim_init(&player);re3c_tim_init(&weapon);
 player.loaded=1u;player.bpp_code=1u;player.has_clut=1u;
 player.image_rect.x=0u;player.image_rect.y=0u;
 player.image_rect.width_words=2u;player.image_rect.height=1u;
 player.clut_rect.x=0u;player.clut_rect.y=480u;
 player.clut_rect.width_words=16u;player.clut_rect.height=1u;
 player.words[0]=0x1201u;player.words[480u*1024u]=0x001Fu;
 weapon.loaded=1u;weapon.bpp_code=1u;weapon.has_clut=1u;
 weapon.image_rect.x=512u;weapon.image_rect.y=0u;
 weapon.image_rect.width_words=1u;weapon.image_rect.height=1u;
 weapon.clut_rect.x=256u;weapon.clut_rect.y=480u;
 weapon.clut_rect.width_words=16u;weapon.clut_rect.height=1u;
 weapon.words[512u]=0x4402u;weapon.words[480u*1024u+256u]=0x03E0u;
 rc=re3p_compose_vram(&merged,&player,&weapon);
 if(rc!=RE3C_OK)return 1;
 if(merged.words[0]!=0x1201u||merged.words[512u]!=0x4402u||
    merged.words[480u*1024u]!=0x001Fu||
    merged.words[480u*1024u+256u]!=0x03E0u)return 2;
 if(merged.words[513u]!=0u||player.words[512u]!=0u)return 3;
 if(re3p_compose_vram(&player,&player,&weapon)!=RE3C_ERR_BAD_ARGUMENT)return 4;
 weapon.image_rect.x=1024u;
 if(re3p_compose_vram(&merged,&player,&weapon)!=RE3C_ERR_RANGE)return 5;
 puts("PASS PLD/PLW VRAM composition, bounds and alias safety");return 0;}
