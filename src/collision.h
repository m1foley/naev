/*
 * See Licensing and Copyright notice in naev.h
 */




#ifndef COLLISION_H
#  define COLLISION_H


#include "opengl.h"
#include "physics.h"


/* Returns 1 if collision is detected */
int CollideSprite( const glTexture* at, const int asx, const int asy, const Vector2d* ap,
      const glTexture* bt, const int bsx, const int bsy, const Vector2d* bp );


#endif /* COLLISION_H */
