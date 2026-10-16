#include "bm_sprite.h"

static BmSpriteBlitter active_blitter;

void bm_sprite_set_blitter(BmSpriteBlitter blitter){active_blitter=blitter;}

void bm_sprite_blit(BmFramebuffer *f,const BmSprite *s,int x,int y){
  if(active_blitter)active_blitter(f,s,x,y);else bm_sprite_blit_c(f,s,x,y);
}

void bm_sprite_blit_c(BmFramebuffer *f,const BmSprite *s,int x,int y){
  int source_x=0,source_y=0;
  int destination_x=x-s->anchor_x,destination_y=y-s->anchor_y;
  int width=s->width,height=s->height,row,column;
  if(destination_x<0){source_x=-destination_x;width-=source_x;destination_x=0;}
  if(destination_y<0){source_y=-destination_y;height-=source_y;destination_y=0;}
  if(destination_x+width>BM_SCREEN_WIDTH)width=BM_SCREEN_WIDTH-destination_x;
  if(destination_y+height>BM_SCREEN_HEIGHT)height=BM_SCREEN_HEIGHT-destination_y;
  if(width<=0||height<=0)return;
  for(row=0;row<height;row++){
    const uint8_t *source=s->pixels+(source_y+row)*s->stride+source_x;
    uint8_t *destination=f->pixels+(destination_y+row)*BM_SCREEN_WIDTH+destination_x;
    for(column=0;column<width;column++)if(source[column])destination[column]=source[column];
  }
}

void bm_sprite_blit_colour(BmFramebuffer *f,const BmSprite *s,int x,int y,
                           uint8_t colour){
  int source_x=0,source_y=0;
  int destination_x=x-s->anchor_x,destination_y=y-s->anchor_y;
  int width=s->width,height=s->height,row,column;
  if(destination_x<0){source_x=-destination_x;width-=source_x;destination_x=0;}
  if(destination_y<0){source_y=-destination_y;height-=source_y;destination_y=0;}
  if(destination_x+width>BM_SCREEN_WIDTH)width=BM_SCREEN_WIDTH-destination_x;
  if(destination_y+height>BM_SCREEN_HEIGHT)height=BM_SCREEN_HEIGHT-destination_y;
  if(width<=0||height<=0)return;
  for(row=0;row<height;row++){
    const uint8_t *source=s->pixels+(source_y+row)*s->stride+source_x;
    uint8_t *destination=f->pixels+(destination_y+row)*BM_SCREEN_WIDTH+destination_x;
    for(column=0;column<width;column++)if(source[column])destination[column]=colour;
  }
}
