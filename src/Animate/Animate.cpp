#include "Animate.h"
#include "config.h"
#include "img/astronaut.h"
#include "img/kiss.h"
#include "img/rub.h"

int Animate_key = -1; //初始化图标显示帧数

void imgAnim(const uint8_t **Animate_value, uint32_t *Animate_size, int Animate_Choice)
{
    Animate_key++;

//太空人起飞
  if(Animate_Choice == 1) {
    if (Animate_key >= 19) {
      if (Animate_key == 21){
        Animate_key = -1;
      } else {
        *Animate_value = rub[19];
        *Animate_size = rub_size[19];
      }
    } else {
      *Animate_value = rub[Animate_key];
      *Animate_size = rub_size[Animate_key];
    }
  } else if(Animate_Choice == 2) {
    *Animate_value = kiss[Animate_key];
    *Animate_size = kiss_size[Animate_key];
    if (Animate_key >= 3)Animate_key = -1;
  } else if(Animate_Choice == 3){

  }

}