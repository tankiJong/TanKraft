#pragma once
#include "Engine/Core/common.hpp"
#include "Game/Gameplay/Entity.hpp"

class Camera;

class Player: public Entity {
public:
  Player();

  virtual void onInput();
  void onUpdate() override;


protected:

};

class Spectator: public Player {
public:
  virtual void onInput() override;
  void onRender() override {}
};