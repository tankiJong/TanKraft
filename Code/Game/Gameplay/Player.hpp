#pragma once
#include "Engine/Core/common.hpp"
#include "Game/Gameplay/Entity.hpp"

class Camera;

class Player: public Entity {
public:
  Player();

  void onInput();
  void onUpdate() override;


protected:

};
