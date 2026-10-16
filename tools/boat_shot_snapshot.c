/* Reuse the classic palette/PPM writer without its scripted entry point. */
#define main classic_snapshot_main
#include "classic_snapshot.c"
#undef main

int main(int argc,char **argv){
  static BmGame game,previous;
  BmInput input={0};
  const char *directory=argc>1?argv[1]:"build/boat-shots";
  unsigned first=0;
  bm_game_init(&game,0x424d4158u);
  while(game.tick<0x3000u){
    previous=game;
    bm_game_tick(&game,&input);
    if(!first&&game.classic_world.gun1_kind==0xbcu&&game.classic_world.gun1_shot_y){
      first=(unsigned)game.tick;
      bm_game_render(&previous);
      if(write_ppm(directory,&previous))return 2;
    }
    if(first&&(game.tick==first||game.tick==first+4u||game.tick==first+12u)){
      bm_game_render(&game);
      if(write_ppm(directory,&game))return 2;
      printf("T%04lX shot=(%u,%u) travel=%u burst=%u gun=%u\n",
        (unsigned long)game.tick,game.classic_world.gun1_shot_x,
        game.classic_world.gun1_shot_y,game.classic_world.gun1_shot_travel,
        game.classic_world.gun1_shot_explosion,game.classic_world.gun1_y);
    }
    if(first&&game.tick==first+12u)return 0;
  }
  fprintf(stderr,"No stationary-boat shot before T3000\n");return 1;
}
