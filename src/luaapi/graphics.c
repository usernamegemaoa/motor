#include <lauxlib.h>
#include "graphics.h"
#include "tools.h"
#include "../graphics/graphics.h"
#include "../graphics/matrixstack.h"
#include "../graphics/image.h"
#include "../graphics/quad.h"
#include "../graphics/font.h"
#include "../graphics/batch.h"
#include "../graphics/canvas.h"
#include "image.h"

static struct {
  int imageMT;
  int quadMT;
  int fontMT;
  int batchMT;
  int canvasMT;

  graphics_Font* currentFont;
  int currentFontRef;
  graphics_Canvas* currentCanvas;
  int currentCanvasRef;
} moduleData;

static int l_graphics_setBackgroundColor(lua_State* state) {
  int red   = lua_tointeger(state, 1);
  int green = lua_tointeger(state, 2);
  int blue  = lua_tointeger(state, 3);
  int alpha = lua_tointeger(state, 4);

  float scale = 1.0f / 255.0f;

  graphics_setBackgroundColor(red * scale, green * scale, blue * scale, alpha * scale);
  return 0;
}

static int l_graphics_setColor(lua_State* state) {
  int red   = lua_tointeger(state, 1);
  int green = lua_tointeger(state, 2);
  int blue  = lua_tointeger(state, 3);
  int alpha = lua_tointeger(state, 4);

  float scale = 1.0f / 255.0f;

  graphics_setColor(red * scale, green * scale, blue * scale, alpha * scale);
  return 0;
}

static int l_graphics_clear(lua_State* state) {
  graphics_clear();
  return 0;
}

typedef struct {
  graphics_Image image;
  int imageDataRef;
} l_graphics_Image;

static int l_graphics_newImage(lua_State* state) {
  if(lua_type(state, 1) == LUA_TSTRING) {
    l_image_newImageData(state);
    lua_remove(state, 1);
  } 
  
  if(!l_image_isImageData(state, 1)) {
    lua_pushstring(state, "expected ImageData");
    return lua_error(state);
  }

  lua_settop(state, 1);
  image_ImageData * imageData = (image_ImageData*)lua_touserdata(state, 1);
  int ref = luaL_ref(state, LUA_REGISTRYINDEX);

  l_graphics_Image *image = (l_graphics_Image*)lua_newuserdata(state, sizeof(l_graphics_Image));

  graphics_Image_new_with_ImageData(&image->image, imageData);
  image->imageDataRef = ref;

  lua_rawgeti(state, LUA_REGISTRYINDEX, moduleData.imageMT);
  lua_setmetatable(state, -2);

  return 1;
}


typedef struct {
  graphics_Batch batch;
  int textureRef;
} l_graphics_Batch;

l_check_type_fn(l_graphics_isBatch, moduleData.batchMT)
l_to_type_fn(l_graphics_toBatch, l_graphics_Batch)
l_check_type_fn(l_graphics_isImage, moduleData.imageMT)
l_to_type_fn(l_graphics_toImage, l_graphics_Image)
l_check_type_fn(l_graphics_isQuad, moduleData.quadMT)
l_to_type_fn(l_graphics_toQuad, graphics_Quad)
l_check_type_fn(l_graphics_isCanvas, moduleData.canvasMT)
l_to_type_fn(l_graphics_toCanvas, graphics_Canvas)


static int l_graphics_gcImage(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "Expected Image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);

  graphics_Image_free(&img->image);
  luaL_unref(state, LUA_REGISTRYINDEX, img->imageDataRef);
  return 0;
}


static const graphics_Quad defaultQuad = {
  .x = 0.0,
  .y = 0.0,
  .w = 1.0,
  .h = 1.0
};
static int l_graphics_draw(lua_State* state) {
  l_graphics_Image const * image = NULL;
  l_graphics_Batch const * batch = NULL;
  graphics_Canvas const * canvas = NULL;
  graphics_Quad const * quad = &defaultQuad;
  int baseidx = 2;

  if(l_graphics_isImage(state, 1)) {
    if(l_graphics_isQuad(state, 2)) {
      quad = l_graphics_toQuad(state, 2);
      baseidx = 3;
    }
    image = l_graphics_toImage(state, 1);
  } else if(l_graphics_isCanvas(state, 1)) {
    if(l_graphics_isQuad(state, 2)) {
      quad = l_graphics_toQuad(state, 2);
      baseidx = 3;
    }
    canvas = l_graphics_toCanvas(state, 1);

  } else if(l_graphics_isBatch(state, 1)) {
    batch = l_graphics_toBatch(state, 1);
  } else {
    lua_pushstring(state, "expected image or spritebatch");
    lua_error(state);
  }
  
  float x  = luaL_optnumber(state, baseidx,     0.0f);
  float y  = luaL_optnumber(state, baseidx + 1, 0.0f);
  float r  = luaL_optnumber(state, baseidx + 2, 0.0f);
  float sx = luaL_optnumber(state, baseidx + 3, 1.0f);
  float sy = luaL_optnumber(state, baseidx + 4, sx);
  float ox = luaL_optnumber(state, baseidx + 5, 0.0f);
  float oy = luaL_optnumber(state, baseidx + 6, 0.0f);
  float kx = luaL_optnumber(state, baseidx + 7, 0.0f);
  float ky = luaL_optnumber(state, baseidx + 8, 0.0f);

  if(image) {
    graphics_Image_draw(&image->image, quad, x, y, r, sx, sy, ox, oy, kx, ky);
  } else if(canvas) {
    graphics_Canvas_draw(canvas, quad, x, y, r, sx, sy, ox, oy, kx, ky);
  } else if(batch) {
    graphics_Batch_draw(&batch->batch, x, y, r, sx, sy, ox, oy, kx, ky);
  }
  return 0;
}

static int l_graphics_push(lua_State* state) {
  if(matrixstack_push()) {
    lua_pushstring(state, "Matrix stack overflow");
    return lua_error(state);
  }
  return 0;
}

static int l_graphics_pop(lua_State* state) {
  if(matrixstack_pop()) {
    lua_pushstring(state, "Matrix stack underrun");
    return lua_error(state);
  }
  return 0;
}

static int l_graphics_translate(lua_State* state) {
  float x = l_tools_tonumber_or_err(state, 1);
  float y = l_tools_tonumber_or_err(state, 2);

  matrixstack_translate(x, y);
  return 0;
}

static int l_graphics_scale(lua_State* state) {
  float x = l_tools_tonumber_or_err(state, 1);
  float y = l_tools_tonumber_or_err(state, 2);

  matrixstack_scale(x, y);
  return 0;
}

static int l_graphics_origin(lua_State* state) {
  matrixstack_origin();
  return 0;
}

static int l_graphics_shear(lua_State* state) {
  lua_pushstring(state, "not implemented");
  lua_error(state);
  return 0;
}

static int l_graphics_rotate(lua_State* state) {
  float a = l_tools_tonumber_or_err(state, 1);

  matrixstack_rotate(a);
  return 0;
}

static int l_graphics_newQuad(lua_State* state) {
  float x = l_tools_tonumber_or_err(state, 1);
  float y = l_tools_tonumber_or_err(state, 2);
  float w = l_tools_tonumber_or_err(state, 3);
  float h = l_tools_tonumber_or_err(state, 4);
  float rw = l_tools_tonumber_or_err(state, 5);
  float rh = l_tools_tonumber_or_err(state, 6);

  graphics_Quad* obj = lua_newuserdata(state, sizeof(graphics_Quad));

  graphics_Quad_new_with_ref(obj, x, y, w, h, rw, rh);

  lua_rawgeti(state, LUA_REGISTRYINDEX, moduleData.quadMT);
  lua_setmetatable(state, -2);

  return 1;
}

static int l_graphics_Image_getDimensions(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);
  lua_pushinteger(state, img->image.width);
  lua_pushinteger(state, img->image.height);
  return 2;
}

static int l_graphics_Image_getWidth(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);
  lua_pushinteger(state, img->image.width);
  return 1;
}

static int l_graphics_Image_getHeight(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);
  lua_pushinteger(state, img->image.height);
  return 1;
}

static int l_graphics_Quad_getViewport(lua_State* state) {
  if(!l_graphics_isQuad(state, 1)) {
    lua_pushstring(state, "expected quad");
    return lua_error(state);
  }

  graphics_Quad *quad = l_graphics_toQuad(state, 1);
  lua_pushnumber(state, quad->x);
  lua_pushnumber(state, quad->y);
  lua_pushnumber(state, quad->w);
  lua_pushnumber(state, quad->h);

  return 4;
}

static int l_graphics_Quad_setViewport(lua_State* state) {
  if(!l_graphics_isQuad(state, 1)) {
    lua_pushstring(state, "expected quad");
    return lua_error(state);
  }

  graphics_Quad *quad = l_graphics_toQuad(state, 1);
  
  float x = l_tools_tonumber_or_err(state, 2);
  float y = l_tools_tonumber_or_err(state, 3);
  float w = l_tools_tonumber_or_err(state, 4);
  float h = l_tools_tonumber_or_err(state, 5);

  quad->x = x;
  quad->y = y;
  quad->w = w;
  quad->h = h;

  return 0;
}

static const l_tools_Enum l_graphics_WrapMode[] = {
  {"clamp", graphics_WrapMode_clamp},
  {"repeat", graphics_WrapMode_repeat},
  {NULL, 0}
};

static int l_graphics_Image_getWrap(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);

  graphics_Wrap wrap;
  graphics_Image_getWrap(&img->image, &wrap);

  l_tools_pushenum(state, wrap.horMode, l_graphics_WrapMode);
  l_tools_pushenum(state, wrap.verMode, l_graphics_WrapMode);

  return 2;
}

static int l_graphics_Image_setWrap(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);
  graphics_Wrap wrap;
  wrap.horMode = l_tools_toenum_or_err(state, 2, l_graphics_WrapMode);
  wrap.verMode = l_tools_toenum_or_err(state, 3, l_graphics_WrapMode);

  graphics_Image_setWrap(&img->image, &wrap);

  return 0;
}

static const l_tools_Enum l_graphics_FilterMode[] = {
  {"nearest", graphics_FilterMode_nearest},
  {"linear",  graphics_FilterMode_linear},
  {NULL, 0}
};

static int l_graphics_Image_getFilter(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);

  graphics_Filter filter;

  graphics_Image_getFilter(&img->image, &filter);

  l_tools_pushenum(state, filter.minMode, l_graphics_FilterMode);
  l_tools_pushenum(state, filter.magMode, l_graphics_FilterMode);
  lua_pushnumber(state, filter.maxAnisotropy);

  return 3;
}

static int l_graphics_Image_setFilter(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);
  graphics_Filter newFilter;
  graphics_Image_getFilter(&img->image, &newFilter);
  newFilter.minMode = l_tools_toenum_or_err(state, 2, l_graphics_FilterMode);
  newFilter.magMode = l_tools_toenum_or_err(state, 3, l_graphics_FilterMode);
  newFilter.maxAnisotropy = luaL_optnumber(state, 4, 1.0f);
  graphics_Image_setFilter(&img->image, &newFilter);

  return 0;
}

static int l_graphics_Image_setMipmapFilter(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);

  graphics_Filter newFilter;
  graphics_Image_getFilter(&img->image, &newFilter);

  if(lua_isnoneornil(state, 2)) {
    newFilter.mipmapMode  = graphics_FilterMode_none; 
    newFilter.mipmapLodBias = 0.0f;
  } else {
    newFilter.mipmapMode  = l_tools_toenum_or_err(state, 2, l_graphics_FilterMode);
    // param 2 is supposed to be "sharpness", which is exactly opposite to LOD,
    // therefore we use the negative value
    newFilter.mipmapLodBias = -luaL_optnumber(state, 3, 0.0f);
  }
  graphics_Image_setFilter(&img->image, &newFilter);

  return 0;
}

static int l_graphics_Image_getMipmapFilter(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);

  graphics_Filter filter;

  graphics_Image_getFilter(&img->image, &filter);

  l_tools_pushenum(state, filter.mipmapMode, l_graphics_FilterMode);
  lua_pushnumber(state, filter.mipmapLodBias);

  return 2;
}

static int l_graphics_Image_getData(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);

  lua_rawgeti(state, LUA_REGISTRYINDEX, img->imageDataRef);

  return 1;
}

static int l_graphics_Image_refresh(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    return lua_error(state);
  }

  l_graphics_Image* img = l_graphics_toImage(state, 1);

  graphics_Image_refresh(&img->image);

  return 0;
}

l_check_type_fn(l_graphics_isFont, moduleData.fontMT)
l_to_type_fn(l_graphics_toFont, graphics_Font)

static int l_graphics_newFont(lua_State* state) {
  // TODO: alternative signatures for newFont
  char const * filename = l_tools_tostring_or_err(state, 1);
  int ptsize = l_tools_tonumber_or_err(state, 2);

  graphics_Font* font = lua_newuserdata(state, sizeof(graphics_Font));
  if(graphics_Font_new(font, filename, ptsize)) {
    lua_pushstring(state, "Could not open font");
    lua_error(state);
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, moduleData.fontMT);
  lua_setmetatable(state, -2);
  return 1;
}

static int l_graphics_gcFont(lua_State* state) {
  graphics_Font* font = l_graphics_toFont(state, 1);
  graphics_Font_free(font);
  return 0;
}

static int l_graphics_setFont(lua_State* state) {
  if(!l_graphics_isFont(state, 1)) {
    lua_pushstring(state, "expected font");
    lua_error(state);
  }

  lua_settop(state, 1);

  graphics_Font* font = l_graphics_toFont(state, 1);

  // Release current font in Lua, so it can be GCed if needed
  if(moduleData.currentFont) {
    luaL_unref(state, LUA_REGISTRYINDEX, moduleData.currentFontRef);
  }

  moduleData.currentFontRef = luaL_ref(state, LUA_REGISTRYINDEX);
  moduleData.currentFont = font;

  return 0;
}

static int l_graphics_Font_getHeight(lua_State* state) {
  if(!l_graphics_isFont(state, 1)) {
    lua_pushstring(state, "expected font");
    lua_error(state);
  }

  graphics_Font* font = l_graphics_toFont(state, 1);

  lua_pushinteger(state, graphics_Font_getHeight(font));
  return 1;
}

static int l_graphics_Font_getDescent(lua_State* state) {
  if(!l_graphics_isFont(state, 1)) {
    lua_pushstring(state, "expected font");
    lua_error(state);
  }

  graphics_Font* font = l_graphics_toFont(state, 1);

  lua_pushinteger(state, graphics_Font_getDescent(font));
  return 1;
}

static int l_graphics_Font_getAscent(lua_State* state) {
  if(!l_graphics_isFont(state, 1)) {
    lua_pushstring(state, "expected font");
    lua_error(state);
  }

  graphics_Font* font = l_graphics_toFont(state, 1);

  lua_pushinteger(state, graphics_Font_getAscent(font));
  return 1;
}

static int l_graphics_Font_getBaseline(lua_State* state) {
  if(!l_graphics_isFont(state, 1)) {
    lua_pushstring(state, "expected font");
    lua_error(state);
  }

  graphics_Font* font = l_graphics_toFont(state, 1);

  lua_pushinteger(state, graphics_Font_getBaseline(font));
  return 1;
}

static int l_graphics_Font_getWidth(lua_State* state) {
  if(!l_graphics_isFont(state, 1)) {
    lua_pushstring(state, "expected font");
    lua_error(state);
  }

  graphics_Font* font = l_graphics_toFont(state, 1);

  char const* line = l_tools_tostring_or_err(state, 2);

  lua_pushinteger(state, graphics_Font_getWidth(font, line));
  return 1;
}

static int l_graphics_Font_getWrap(lua_State* state) {
  if(!l_graphics_isFont(state, 1)) {
    lua_pushstring(state, "expected font");
    lua_error(state);
  }

  graphics_Font* font = l_graphics_toFont(state, 1);

  char const* line = l_tools_tostring_or_err(state, 2);
  int width = l_tools_tonumber_or_err(state, 3);

  lua_pushinteger(state, graphics_Font_getWrap(font, line, width, NULL));
  return 1;
}

static int l_graphics_printf(lua_State* state) {
  char const* text = l_tools_tostring_or_err(state, 1);
  return 0;
}

static int l_graphics_print(lua_State* state) {
  char const* text = l_tools_tostring_or_err(state, 1);
  int x = l_tools_tonumber_or_err(state, 2);
  int y = l_tools_tonumber_or_err(state, 3);

  graphics_Font_render(moduleData.currentFont, text);
  return 0;
}

static int l_graphics_newSpriteBatch(lua_State* state) {
  if(!l_graphics_isImage(state, 1)) {
    lua_pushstring(state, "expected image");
    lua_error(state);
  }

  l_graphics_Image const* image = l_graphics_toImage(state, 1);
  int count = luaL_optnumber(state, 2, 128);

  l_graphics_Batch* batch = lua_newuserdata(state, sizeof(l_graphics_Batch));
  graphics_Batch_new(&batch->batch, &image->image, count, graphics_BatchUsage_static);

  lua_pushvalue(state, 1);
  batch->textureRef = luaL_ref(state, LUA_REGISTRYINDEX);

  lua_rawgeti(state, LUA_REGISTRYINDEX, moduleData.batchMT);
  lua_setmetatable(state, -2);
  return 1;
}

static int l_graphics_gcSpriteBatch(lua_State* state) {
  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);
  graphics_Batch_free(&batch->batch);
  luaL_unref(state, LUA_REGISTRYINDEX, batch->textureRef);


  return 0;
}

static int l_graphics_SpriteBatch_bind(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  graphics_Batch_bind(&batch->batch);

  return 0;
}

static int l_graphics_SpriteBatch_unbind(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  graphics_Batch_unbind(&batch->batch);

  return 0;
}

static int l_graphics_SpriteBatch_add(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  graphics_Quad const * quad = &defaultQuad;
  int baseidx = 2;

  if(l_graphics_isQuad(state, 2)) {
    quad = l_graphics_toQuad(state, 2);
    baseidx = 3;
  }
  
  float x  = luaL_optnumber(state, baseidx,     0.0f);
  float y  = luaL_optnumber(state, baseidx + 1, 0.0f);
  float r  = luaL_optnumber(state, baseidx + 2, 0.0f);
  float sx = luaL_optnumber(state, baseidx + 3, 1.0f);
  float sy = luaL_optnumber(state, baseidx + 4, sx);
  float ox = luaL_optnumber(state, baseidx + 5, 0.0f);
  float oy = luaL_optnumber(state, baseidx + 6, 0.0f);
  float kx = luaL_optnumber(state, baseidx + 7, 0.0f);
  float ky = luaL_optnumber(state, baseidx + 8, 0.0f);

  int i = graphics_Batch_add(&batch->batch, quad, x, y, r, sx, sy, ox, oy, kx, ky);
  lua_pushinteger(state, i);


  return 1;
}

static int l_graphics_SpriteBatch_set(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  int id = l_tools_tonumber_or_err(state, 2);

  graphics_Quad const * quad = &defaultQuad;
  int baseidx = 3;

  if(l_graphics_isQuad(state, 3)) {
    quad = l_graphics_toQuad(state, 3);
    baseidx = 4;
  }
  
  float x  = luaL_optnumber(state, baseidx,     0.0f);
  float y  = luaL_optnumber(state, baseidx + 1, 0.0f);
  float r  = luaL_optnumber(state, baseidx + 2, 0.0f);
  float sx = luaL_optnumber(state, baseidx + 3, 1.0f);
  float sy = luaL_optnumber(state, baseidx + 4, sx);
  float ox = luaL_optnumber(state, baseidx + 5, 0.0f);
  float oy = luaL_optnumber(state, baseidx + 6, 0.0f);
  float kx = luaL_optnumber(state, baseidx + 7, 0.0f);
  float ky = luaL_optnumber(state, baseidx + 8, 0.0f);

  graphics_Batch_set(&batch->batch, id, quad, x, y, r, sx, sy, ox, oy, kx, ky);

  return 0;
}

static int l_graphics_SpriteBatch_clear(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);
  graphics_Batch_clear(&batch->batch);

  return 0;
}

int l_graphics_SpriteBatch_getBufferSize(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);
  
  lua_pushnumber(state, batch->batch.maxCount);

  return 1;
}

int l_graphics_SpriteBatch_setBufferSize(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  int newsize = l_tools_tonumber_or_err(state, 2);
  
  graphics_Batch_setBufferSize(&batch->batch, newsize);

  return 0;
}

int l_graphics_SpriteBatch_getCount(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  lua_pushnumber(state, batch->batch.insertPos);

  return 1;
}

int l_graphics_SpriteBatch_setTexture(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  if(!l_graphics_isImage(state, 2)) {
    lua_pushstring(state, "expected image");
    lua_error(state);
  }

  l_graphics_Image * image = l_graphics_toImage(state, 2);
  batch->batch.texture = &image->image;
  luaL_unref(state, LUA_REGISTRYINDEX, batch->textureRef);
  lua_settop(state, 2);
  batch->textureRef = luaL_ref(state, LUA_REGISTRYINDEX);

  return 0;
}

int l_graphics_SpriteBatch_getTexture(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  lua_rawgeti(state, LUA_REGISTRYINDEX, batch->textureRef);

  return 1;
}

int l_graphics_SpriteBatch_setColor(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  if(!lua_isnumber(state, 2)) {
    graphics_Batch_clearColor(&batch->batch);
  } else {
    float r = l_tools_tonumber_or_err(state, 2);
    float g = l_tools_tonumber_or_err(state, 3);
    float b = l_tools_tonumber_or_err(state, 4);
    float a = luaL_optnumber(state, 5, 255);
    graphics_Batch_setColor(&batch->batch, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
  }

  
  return 0;
}

int l_graphics_SpriteBatch_getColor(lua_State* state) {
  if(!l_graphics_isBatch(state, 1)) {
    lua_pushstring(state, "expected batch");
    lua_error(state);
  }

  l_graphics_Batch * batch = l_graphics_toBatch(state, 1);

  if(!batch->batch.colorSet) {
    return 0;
  }

  lua_pushnumber(state, floor(batch->batch.color.x * 255));
  lua_pushnumber(state, floor(batch->batch.color.y * 255));
  lua_pushnumber(state, floor(batch->batch.color.z * 255));
  lua_pushnumber(state, floor(batch->batch.color.w * 255));

  return 4;
}

static int l_graphics_newCanvas(lua_State* state) {
  // TODO support default parameters

  int width = l_tools_tonumber_or_err(state, 1);
  int height = l_tools_tonumber_or_err(state, 2);
  
  graphics_Canvas *canvas = lua_newuserdata(state, sizeof(graphics_Canvas));
  graphics_Canvas_new(canvas, width, height);

  lua_rawgeti(state, LUA_REGISTRYINDEX, moduleData.canvasMT);
  lua_setmetatable(state, -2);

  return 1;
}

static int l_graphics_gcCanvas(lua_State* state) {
  if(!l_graphics_isCanvas(state, 1)) {
    lua_pushstring(state, "expected canvas");
    return lua_error(state);
  }

  graphics_Canvas *canvas = l_graphics_toCanvas(state, 1);

  graphics_Canvas_free(canvas);

  return 1;
}

static int l_graphics_setCanvas(lua_State* state) {
  graphics_Canvas *canvas = NULL;

  if(l_graphics_isCanvas(state, 1)) {
    canvas = l_graphics_toCanvas(state, 1);
  } else if(!lua_isnoneornil(state, 1)) {
    lua_pushstring(state, "expected none or canvas");
    return lua_error(state);
  }

  if(moduleData.currentCanvas) {
    moduleData.currentCanvas = NULL;
    luaL_unref(state, LUA_REGISTRYINDEX, moduleData.currentFontRef);
  }

  // TODO support multiple canvases
  lua_settop(state, 1);

  graphics_setCanvas(canvas);
  if(canvas) {
    moduleData.currentCanvas = canvas;
    moduleData.currentFontRef = luaL_ref(state, LUA_REGISTRYINDEX);
  }

  return 0;
}

static int l_graphics_setColorMask(lua_State* state) {
  if(lua_isnone(state, 1)) {
    graphics_setColorMask(true, true, true, true);
    return 0;
  } else {
    for(int i = 2; i < 5; ++i) {
      if(lua_isnone(state, i)) {
        lua_pushstring(state, "illegal paramters");
        return lua_error(state);
      }
    }
  }

  bool r = lua_toboolean(state, 1);
  bool g = lua_toboolean(state, 2);
  bool b = lua_toboolean(state, 3);
  bool a = lua_toboolean(state, 4);

  graphics_setColorMask(r,g,b,a);

  return 0;
}

static int l_graphics_getColorMask(lua_State* state) {
  bool r,g,b,a;
  graphics_getColorMask(&r, &g, &b, &a);
  lua_pushboolean(state, r);
  lua_pushboolean(state, g);
  lua_pushboolean(state, b);
  lua_pushboolean(state, a);

  return 4;
}

static const l_tools_Enum l_graphics_BlendMode[] = {
  {"additive",       graphics_BlendMode_additive},
  {"alpha",          graphics_BlendMode_alpha},
  {"subtractive",    graphics_BlendMode_subtractive},
  {"multiplicative", graphics_BlendMode_multiplicative},
  {"premultiplied",  graphics_BlendMode_premultiplied},
  {"replace",        graphics_BlendMode_replace},
  {"screen",         graphics_BlendMode_screen},
  {NULL, 0}
};

static int l_graphics_setBlendMode(lua_State* state) {
  graphics_BlendMode mode = l_tools_toenum_or_err(state, 1, l_graphics_BlendMode);
  graphics_setBlendMode(mode);
  return 0;
}

static int l_graphics_getBlendMode(lua_State* state) {
  l_tools_pushenum(state, graphics_getBlendMode(), l_graphics_BlendMode);
  return 1;
}

static int l_graphics_setScissor(lua_State* state) {
  if(lua_isnone(state, 1)) {
    graphics_clearScissor();
    return 0;
  } else {
    for(int i = 2; i < 5; ++i) {
      if(lua_isnone(state, i)) {
        lua_pushstring(state, "illegal paramters");
        return lua_error(state);
      }
    }
  }

  int x = l_tools_tonumber_or_err(state, 1);
  int y = l_tools_tonumber_or_err(state, 2);
  int w = l_tools_tonumber_or_err(state, 3);
  int h = l_tools_tonumber_or_err(state, 4);

  graphics_setScissor(x,y,w,h);

  return 0;
}

static int l_graphics_getScissor(lua_State* state) {
  int x,y,w,h;
  bool scissor = graphics_getScissor(&x, &y, &w, &h);
  if(!scissor) {
    return 0;
  }

  lua_pushinteger(state, x);
  lua_pushinteger(state, y);
  lua_pushinteger(state, w);
  lua_pushinteger(state, h);

  return 4;
}

static luaL_Reg const regFuncs[] = {
  {"setBackgroundColor", l_graphics_setBackgroundColor},
  {"setColor",           l_graphics_setColor},
  {"clear",              l_graphics_clear},
  {"newImage",           l_graphics_newImage},
  {"newQuad",            l_graphics_newQuad},
  {"draw",               l_graphics_draw},
  {"push",               l_graphics_push},
  {"pop",                l_graphics_pop},
  {"origin",             l_graphics_origin},
  {"rotate",             l_graphics_rotate},
  {"scale",              l_graphics_scale},
  {"shear",              l_graphics_shear},
  {"translate",          l_graphics_translate},
  {"newFont",            l_graphics_newFont},
  {"setFont",            l_graphics_setFont},
  {"printf",             l_graphics_printf},
  {"print",              l_graphics_print},
  {"newSpriteBatch",     l_graphics_newSpriteBatch},
  {"newCanvas",          l_graphics_newCanvas},
  {"setCanvas",          l_graphics_setCanvas},
  {"setColorMask",       l_graphics_setColorMask},
  {"getColorMask",       l_graphics_getColorMask},
  {"setBlendMode",       l_graphics_setBlendMode},
  {"getBlendMode",       l_graphics_getBlendMode},
  {"setScissor",         l_graphics_setScissor},
  {"getScissor",         l_graphics_getScissor},
  {NULL, NULL}
};

static luaL_Reg const imageMetatableFuncs[] = {
  {"__gc",               l_graphics_gcImage},
  {"getDimensions",      l_graphics_Image_getDimensions},
  {"getWidth",           l_graphics_Image_getWidth},
  {"getHeight",          l_graphics_Image_getHeight},
  {"setFilter",          l_graphics_Image_setFilter},
  {"getFilter",          l_graphics_Image_getFilter},
  {"setMipmapFilter",    l_graphics_Image_setMipmapFilter},
  {"getMipmapFilter",    l_graphics_Image_getMipmapFilter},
  {"setWrap",            l_graphics_Image_setWrap},
  {"getWrap",            l_graphics_Image_getWrap},
  {"getData",            l_graphics_Image_getData},
  {"refresh",            l_graphics_Image_refresh},
  {NULL, NULL}
};

static luaL_Reg const quadMetatableFuncs[] = {
  {"getViewport",        l_graphics_Quad_getViewport},
  {"setViewport",        l_graphics_Quad_setViewport},
  {NULL, NULL}
};

static luaL_Reg const fontMetatableFuncs[] = {
  {"__gc",               l_graphics_gcFont},
  {"getHeight",          l_graphics_Font_getHeight},
  {"getAscent",          l_graphics_Font_getAscent},
  {"getDescent",         l_graphics_Font_getDescent},
  {"getBaseline",        l_graphics_Font_getBaseline},
  {"getWidth",           l_graphics_Font_getWidth},
  {"getWrap",            l_graphics_Font_getWrap},
  {NULL, NULL}
};

static luaL_Reg const batchMetatableFuncs[] = {
  {"__gc",               l_graphics_gcSpriteBatch},
  {"add",                l_graphics_SpriteBatch_add},
  {"set",                l_graphics_SpriteBatch_set},
  {"bind",               l_graphics_SpriteBatch_bind},
  {"unbind",             l_graphics_SpriteBatch_unbind},
  {"clear",              l_graphics_SpriteBatch_clear},
  {"getBufferSize",      l_graphics_SpriteBatch_getBufferSize},
  {"setBufferSize",      l_graphics_SpriteBatch_setBufferSize},
  {"getCount",           l_graphics_SpriteBatch_getCount},
  {"setTexture",         l_graphics_SpriteBatch_setTexture},
  {"setImage",           l_graphics_SpriteBatch_setTexture},
  {"getTexture",         l_graphics_SpriteBatch_getTexture},
  {"getImage",           l_graphics_SpriteBatch_getTexture},
  {"setColor",           l_graphics_SpriteBatch_setColor},
  {"getColor",           l_graphics_SpriteBatch_getColor},
  {NULL, NULL}
};

static luaL_Reg const canvasMetatableFuncs[] = {
  {"__gc",               l_graphics_gcCanvas},
  {NULL, NULL}
};

int l_graphics_register(lua_State* state) {
  l_tools_register_module(state, "graphics", regFuncs);

  moduleData.imageMT  = l_tools_make_type_mt(state, imageMetatableFuncs);
  moduleData.quadMT   = l_tools_make_type_mt(state, quadMetatableFuncs);
  moduleData.fontMT   = l_tools_make_type_mt(state, fontMetatableFuncs);
  moduleData.batchMT  = l_tools_make_type_mt(state, batchMetatableFuncs);
  moduleData.canvasMT = l_tools_make_type_mt(state, canvasMetatableFuncs);
  moduleData.currentFont = NULL;

  return 0;
}
