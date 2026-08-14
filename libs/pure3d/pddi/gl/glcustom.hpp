//=============================================================================
// Linux port: user material shaders (OpenGL 3.3 core renderer).
//=============================================================================

// GLSL files assigned to materials by shaders/materials.ini (in the shaders folder: shaders/ in the working directory
// or $SHAR_SHADERS) and by a mod's Shaders/materials.ini; the user's rules come first. A material matches by its own
// name from the art, the art file it was loaded from, or the name of its pddi shader type ("simple", "spheremap", a
// mod's own), in that order. Files reload when they change. Format: shaders/README.md.

#ifndef _GLCUSTOM_HPP_
#define _GLCUSTOM_HPP_

class pglPipeline;

// from BeginFrame (render thread): loads the rules and shaders the first time, then reloads changed ones
void pglCustomUpdate(pglPipeline* pipeline);

// bumped whenever the rules or shaders are reloaded: materials look themselves up again
unsigned pglCustomGeneration();

const int pglMaxMaterialPasses = 4;

// the passes a material is drawn in: the custom program slot of each (pglPipeline::Material::custom), -1 for the
// built-in drawing; returns the count, 1 when no rule matches (slots[0] = -1). A shader that did not build is the
// built-in drawing in the first pass and left out in later ones.
int pglCustomShadersFor(const char* type, const char* material, const char* file, int* slots);

#endif
