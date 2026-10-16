#include "bm_collision.h"
#include "bm_chbas_assets.h"

/* For now every mask is the original sprite or charset itself; an upgraded
   renderer replaces only the bm_asset_* images it draws. */

const uint8_t *bm_mask_player(int8_t bank,uint8_t frame){
  if(bank<0)return frame?bm_asset_bank_left_frame_0:bm_asset_bank_left_frame_1;
  if(bank>0)return frame?bm_asset_bank_right_frame_0:bm_asset_bank_right_frame_1;
  return frame?bm_asset_plane_frame_0:bm_asset_plane_frame_1;
}

const uint8_t *bm_mask_enemy_fighter(uint8_t forward,uint8_t turn,int *height,
                                     uint8_t *p2_value){
  *p2_value=0u;
  if(!forward&&turn==1u){
    *height=BM_ASSET_BANK_LEFT_FRAME_1_HEIGHT;return bm_asset_bank_left_frame_1;
  }
  if(!forward&&turn==2u){
    *height=BM_ASSET_BANK_RIGHT_FRAME_1_HEIGHT;return bm_asset_bank_right_frame_1;
  }
  *p2_value=10u;
  if(forward&&turn==1u){
    *height=BM_ASSET_FIGHTER_LEFT_HEIGHT;*p2_value=0u;return bm_asset_fighter_left;
  }
  if(forward&&turn==2u){
    *height=BM_ASSET_FIGHTER_RIGHT_HEIGHT;*p2_value=0u;return bm_asset_fighter_right;
  }
  if(forward){*height=BM_ASSET_FIGHTER_FORWARD_HEIGHT;return bm_asset_fighter_forward;}
  *height=BM_ASSET_FIGHTER_BACK_HEIGHT;return bm_asset_fighter_back;
}

uint8_t bm_mask_ship(int x,int y){
  if(x<0||x>=BM_MASK_WIDTH)return 0u;
  return (uint8_t)((y>=0&&y<BM_ASSET_SHIP_LAYER_0_HEIGHT&&
                    bm_asset_ship_layer_0[y*16+x])||
                   (y>=2&&y<2+BM_ASSET_SHIP_LAYER_1_HEIGHT&&
                    bm_asset_ship_layer_1[(y-2)*16+x]));
}

void bm_mask_vehicle(uint8_t truck,uint8_t left,const uint8_t **body,
                     const uint8_t **tyres,int *height){
  if(truck){
    *body=left?bm_asset_truck_left_body:bm_asset_truck_right_body;
    *tyres=left?bm_asset_truck_left_tyres:bm_asset_truck_right_tyres;
    *height=6;
  }else{
    *body=left?bm_asset_car_left_body:bm_asset_car_right_body;
    *tyres=left?bm_asset_car_left_tyres:bm_asset_car_right_tyres;
    *height=5;
  }
}

const uint8_t *bm_mask_shadow(void){return bm_asset_shadow;}

const uint8_t *bm_mask_playfield_charset(uint8_t tank_charset_loaded){
  return tank_charset_loaded?bm_asset_max3_tank_charset:bm_asset_max3_hangar_charset;
}

/* The mask sizes the game relies on. */
typedef char bm_mask_shadow_height_check[
  (int)BM_ASSET_SHADOW_HEIGHT==(int)BM_MASK_SHADOW_HEIGHT?1:-1];
typedef char bm_mask_plane_height_check[
  (int)BM_ASSET_PLANE_FRAME_0_HEIGHT==(int)BM_MASK_PLANE_HEIGHT&&
  (int)BM_ASSET_BANK_LEFT_FRAME_0_HEIGHT==(int)BM_MASK_PLANE_HEIGHT?1:-1];
