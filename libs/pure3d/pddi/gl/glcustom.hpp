//=============================================================================
// Linux port: user material shaders (OpenGL 3.3 core renderer).
//=============================================================================

// GLSL files assigned to materials by shaders/materials.ini (in the shaders folder: shaders/ in the working directory
// or $SHAR_SHADERS) and by a mod's Shaders/materials.ini; the user's rules come first. A material matches by the name
// of its pddi shader type ("simple", "spheremap", a mod's own) or by its own name from the art. Files reload when
// they change. Format: shaders/README.md.

#ifndef _GLCUSTOM_HPP_
#define _GLCUSTOM_HPP_

class pglPipeline;

// from BeginFrame (render thread): loads the rules and shaders the first time, then reloads changed ones
void pglCustomUpdate(pglPipeline* pipeline);

// bumped whenever the rules or shaders are reloaded: materials look themselves up again
unsigned pglCustomGeneration();

// the custom program slot for a material (pglPipeline::Material::custom), or -1
int pglCustomShaderFor(const char* type, const char* material);

#endif
