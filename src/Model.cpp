#include "Model.h"
#include "Shader.h"
#include "Texture.h"

// Tell tinygltf NOT to pull in stb_image here (already provided globally)
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <stb_image.h>
#include <tiny_gltf.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <functional>
#include <cmath>

// Model::~Model
// Purpose: Clean up all GPU resources owned by this model.
Model::~Model() { clear(); }

// Model::clear
// Purpose: Delete all VAOs, VBOs, EBOs, and GL textures, then reset all fields so the
//          model can be reused or safely destroyed.
void Model::clear() {
    for (auto& p : primitives) {
        if (p.vao) glDeleteVertexArrays(1, &p.vao);
        if (p.vbo) glDeleteBuffers(1, &p.vbo);
        if (p.ebo) glDeleteBuffers(1, &p.ebo);
    }
    primitives.clear();
    for (GLuint t : textures) glDeleteTextures(1, &t);
    textures.clear();
    if (whiteTex) { glDeleteTextures(1, &whiteTex); whiteTex = 0; }
    loaded = false;
}

// nodeLocalMatrix
// Purpose: Compute the local-space transform matrix for a glTF scene node.
// Inputs:  node - a tinygltf node that may carry a 4x4 matrix or TRS decomposition
// Returns: 4x4 transform matrix; uses the node's matrix field directly if present,
//          otherwise composes translation * rotation * scale from their individual fields.
static glm::mat4 nodeLocalMatrix(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        glm::mat4 m(1.0f);
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                m[c][r] = (float)node.matrix[c * 4 + r];
        return m;
    }
    glm::mat4 T(1.0f), R(1.0f), S(1.0f);
    if (node.translation.size() == 3)
        T = glm::translate(glm::mat4(1.0f),
                           glm::vec3((float)node.translation[0],
                                     (float)node.translation[1],
                                     (float)node.translation[2]));
    if (node.rotation.size() == 4) {
        glm::quat q((float)node.rotation[3], (float)node.rotation[0],
                    (float)node.rotation[1], (float)node.rotation[2]);
        R = glm::mat4_cast(q);
    }
    if (node.scale.size() == 3)
        S = glm::scale(glm::mat4(1.0f),
                       glm::vec3((float)node.scale[0],
                                 (float)node.scale[1],
                                 (float)node.scale[2]));
    return T * R * S;
}

// uploadImage
// Purpose: Upload a decoded glTF image to a new OpenGL 2D texture with mipmaps.
// Inputs:  img - tinygltf image containing width, height, component count, and raw pixel data
// Returns: GL texture handle, or 0 if the image data is empty or has invalid dimensions.
static GLuint uploadImage(const tinygltf::Image& img) {
    if (img.width <= 0 || img.height <= 0 || img.image.empty()) return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum fmt = (img.component == 4) ? GL_RGBA
               : (img.component == 3) ? GL_RGB
               : GL_RED;
    GLenum ifmt = (img.component == 4) ? GL_SRGB_ALPHA
                : (img.component == 3) ? GL_SRGB
                : GL_RED;
    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, img.width, img.height, 0,
                 fmt, GL_UNSIGNED_BYTE, img.image.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

// accessorData
// Purpose: Resolve a glTF accessor index to a raw byte pointer with stride and element count.
// Inputs:  m       - the tinygltf model containing all accessors, buffer views, and buffers
//          accIdx  - index of the accessor to resolve
// Outputs: stride      - byte stride between consecutive elements
//          count       - number of elements in the accessor
//          compsOut    - number of scalar components per element (1/2/3/4)
//          compTypeOut - GL component type (e.g., TINYGLTF_COMPONENT_TYPE_FLOAT)
// Returns: Pointer to the first byte of the accessor's data in the underlying buffer.
static const unsigned char* accessorData(const tinygltf::Model& m, int accIdx,
                                         size_t& stride, size_t& count,
                                         int& compsOut, int& compTypeOut) {
    const auto& acc = m.accessors[accIdx];
    const auto& bv  = m.bufferViews[acc.bufferView];
    const auto& buf = m.buffers[bv.buffer];
    int comps = 1;
    switch (acc.type) {
        case TINYGLTF_TYPE_SCALAR: comps = 1; break;
        case TINYGLTF_TYPE_VEC2:   comps = 2; break;
        case TINYGLTF_TYPE_VEC3:   comps = 3; break;
        case TINYGLTF_TYPE_VEC4:   comps = 4; break;
        default: comps = 1;
    }
    int compSize = 4;
    switch (acc.componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:          compSize = 4; break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   compSize = 4; break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: compSize = 2; break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:          compSize = 2; break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  compSize = 1; break;
        case TINYGLTF_COMPONENT_TYPE_BYTE:           compSize = 1; break;
    }
    stride = (bv.byteStride != 0) ? bv.byteStride : (size_t)(comps * compSize);
    count = acc.count;
    compsOut = comps;
    compTypeOut = acc.componentType;
    return buf.data.data() + bv.byteOffset + acc.byteOffset;
}

// Model::loadFromFile
// Purpose: Parse a .gltf or .glb file and upload all geometry and textures to the GPU.
// Inputs:  path     - file path of the glTF asset to load
//          flipUVs  - if true, flips the V texture coordinate (needed for some exporters)
// Returns: true on success; false if loading or parsing fails (error printed to stderr).
// Actions: Clears any existing data, loads via tinygltf, uploads images, walks the scene
//          graph to build interleaved vertex buffers per primitive (pos/norm/uv), optionally
//          uploads index buffers, resolves materials, and computes modelRadius from the AABB.
bool Model::loadFromFile(const std::string& path, bool flipUVs) {
    clear();

    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool ok = false;

    bool isBinary = false;
    if (path.size() >= 4) {
        std::string ext = path.substr(path.size() - 4);
        for (auto& c : ext) c = (char)tolower(c);
        isBinary = (ext == ".glb");
    }
    if (isBinary) ok = loader.LoadBinaryFromFile(&gltf, &err, &warn, path);
    else           ok = loader.LoadASCIIFromFile(&gltf, &err, &warn, path);

    if (!warn.empty()) std::cerr << "[Model] Warning: " << warn << "\n";
    if (!ok) {
        std::cerr << "[Model] Failed to load \"" << path << "\": " << err << "\n";
        return false;
    }

    // Create white 1x1 fallback
    whiteTex = makeSolidTexture(255, 255, 255);

    // Upload all images as textures
    std::vector<GLuint> texByImage(gltf.images.size(), 0);
    for (size_t i = 0; i < gltf.images.size(); ++i) {
        texByImage[i] = uploadImage(gltf.images[i]);
        if (texByImage[i]) textures.push_back(texByImage[i]);
    }

    // Map gltf textures -> GL handles
    auto textureHandleFor = [&](int gltfTexIdx) -> GLuint {
        if (gltfTexIdx < 0 || gltfTexIdx >= (int)gltf.textures.size()) return 0;
        int imgIdx = gltf.textures[gltfTexIdx].source;
        if (imgIdx < 0 || imgIdx >= (int)texByImage.size()) return 0;
        return texByImage[imgIdx];
    };

    // Walk scene graph and emit primitives
    glm::vec3 aabbMin( 1e30f), aabbMax(-1e30f);

    std::function<void(int, const glm::mat4&)> processNode =
        [&](int nodeIdx, const glm::mat4& parentTransform) {
        if (nodeIdx < 0 || nodeIdx >= (int)gltf.nodes.size()) return;
        const auto& node = gltf.nodes[nodeIdx];
        glm::mat4 worldT = parentTransform * nodeLocalMatrix(node);

        if (node.mesh >= 0 && node.mesh < (int)gltf.meshes.size()) {
            const auto& mesh = gltf.meshes[node.mesh];
            for (const auto& prim : mesh.primitives) {
                Primitive out;
                out.bakedTransform = worldT;

                // Find attributes
                auto itPos = prim.attributes.find("POSITION");
                if (itPos == prim.attributes.end()) continue;

                size_t stride=0, count=0; int comps=0, ctype=0;
                const unsigned char* posData =
                    accessorData(gltf, itPos->second, stride, count, comps, ctype);

                // Build interleaved pos(3) + norm(3) + uv(2)
                std::vector<float> verts(count * 8, 0.0f);
                for (size_t i = 0; i < count; ++i) {
                    const float* p = (const float*)(posData + i * stride);
                    verts[i*8 + 0] = p[0];
                    verts[i*8 + 1] = p[1];
                    verts[i*8 + 2] = p[2];
                    verts[i*8 + 6] = 0.0f; // default UV
                    verts[i*8 + 7] = 0.0f;
                    // Expand AABB using the baked transform
                    glm::vec4 pw = worldT * glm::vec4(p[0], p[1], p[2], 1.0f);
                    aabbMin = glm::min(aabbMin, glm::vec3(pw));
                    aabbMax = glm::max(aabbMax, glm::vec3(pw));
                }

                // Normals (optional)
                auto itN = prim.attributes.find("NORMAL");
                if (itN != prim.attributes.end()) {
                    size_t ns=0, nc=0; int ncomp=0, nct=0;
                    const unsigned char* nd = accessorData(gltf, itN->second, ns, nc, ncomp, nct);
                    for (size_t i = 0; i < count && i < nc; ++i) {
                        const float* nv = (const float*)(nd + i * ns);
                        verts[i*8 + 3] = nv[0];
                        verts[i*8 + 4] = nv[1];
                        verts[i*8 + 5] = nv[2];
                    }
                } else {
                    // Fallback: approximate normals as position-normalized (spherical)
                    for (size_t i = 0; i < count; ++i) {
                        float x = verts[i*8+0], y = verts[i*8+1], z = verts[i*8+2];
                        float len = std::sqrt(x*x + y*y + z*z);
                        if (len > 0.0f) { x/=len; y/=len; z/=len; }
                        verts[i*8 + 3] = x;
                        verts[i*8 + 4] = y;
                        verts[i*8 + 5] = z;
                    }
                }

                // UVs (optional)
                auto itT = prim.attributes.find("TEXCOORD_0");
                if (itT != prim.attributes.end()) {
                    size_t ts=0, tc=0; int tcomp=0, tct=0;
                    const unsigned char* td = accessorData(gltf, itT->second, ts, tc, tcomp, tct);
                    for (size_t i = 0; i < count && i < tc; ++i) {
                        const float* uv = (const float*)(td + i * ts);
                        verts[i*8 + 6] = uv[0];
                        verts[i*8 + 7] = flipUVs ? 1.0f - uv[1] : uv[1];
                    }
                }

                // Upload vertex buffer
                glGenVertexArrays(1, &out.vao);
                glGenBuffers(1, &out.vbo);
                glBindVertexArray(out.vao);
                glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
                glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(float),
                             verts.data(), GL_STATIC_DRAW);
                GLsizei str = 8 * sizeof(float);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, str, (void*)0);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, str, (void*)(3*sizeof(float)));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, str, (void*)(6*sizeof(float)));
                glEnableVertexAttribArray(2);

                // Indices (optional)
                if (prim.indices >= 0) {
                    const auto& iacc = gltf.accessors[prim.indices];
                    const auto& ibv  = gltf.bufferViews[iacc.bufferView];
                    const auto& ibuf = gltf.buffers[ibv.buffer];
                    glGenBuffers(1, &out.ebo);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                 ibv.byteLength - iacc.byteOffset,
                                 ibuf.data.data() + ibv.byteOffset + iacc.byteOffset,
                                 GL_STATIC_DRAW);
                    out.indexCount = (GLsizei)iacc.count;
                    out.indexed = true;
                    switch (iacc.componentType) {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                            out.indexType = GL_UNSIGNED_INT; break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                            out.indexType = GL_UNSIGNED_SHORT; break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                            out.indexType = GL_UNSIGNED_BYTE; break;
                        default:
                            out.indexType = GL_UNSIGNED_INT; break;
                    }
                } else {
                    out.indexed = false;
                    out.vertexCount = (GLsizei)count;
                }
                glBindVertexArray(0);

                // Material
                if (prim.material >= 0 && prim.material < (int)gltf.materials.size()) {
                    const auto& mat = gltf.materials[prim.material];
                    const auto& bcf = mat.pbrMetallicRoughness.baseColorFactor;
                    if (bcf.size() == 4) {
                        out.baseColorFactor = glm::vec4(
                            (float)bcf[0], (float)bcf[1], (float)bcf[2], (float)bcf[3]);
                    }
                    int bcTex = mat.pbrMetallicRoughness.baseColorTexture.index;
                    out.baseColorTex = textureHandleFor(bcTex);
                }

                primitives.push_back(out);
            }
        }

        for (int child : node.children) processNode(child, worldT);
    };

    int sceneIdx = gltf.defaultScene >= 0 ? gltf.defaultScene : 0;
    if (sceneIdx < (int)gltf.scenes.size()) {
        for (int n : gltf.scenes[sceneIdx].nodes) processNode(n, glm::mat4(1.0f));
    } else if (!gltf.nodes.empty()) {
        processNode(0, glm::mat4(1.0f));
    }

    // Compute bounding radius (half-diagonal of AABB)
    if (aabbMax.x > aabbMin.x) {
        glm::vec3 ext = aabbMax - aabbMin;
        modelRadius = glm::length(ext) * 0.5f;
        if (modelRadius < 1e-4f) modelRadius = 1.0f;
    }

    std::cout << "[Model] Loaded \"" << path << "\": "
              << primitives.size() << " primitives, "
              << textures.size() << " textures, radius ~"
              << modelRadius << "\n";

    loaded = true;
    return true;
}

// Model::draw
// Purpose: Draw every primitive in the model using the provided planet shader.
// Inputs:  planetShader - shader with uModel/uNormalMat/uTexture uniforms already bound
//          parentModel  - parent transform; each primitive's model matrix =
//                         parentModel * primitive.bakedTransform
// Actions: Iterates all primitives, computes the full model matrix and normal matrix,
//          binds the base-color texture (or whiteTex fallback), and issues a draw call.
//          Face culling is temporarily disabled to handle double-sided glTF materials.
void Model::draw(Shader& planetShader, const glm::mat4& parentModel) const {
    planetShader.setInt("uTexture", 0);

    // The glTF materials exported from UE5 set "doubleSided": true and their
    // triangle winding doesn't match what back-face culling expects - which
    // results in only the FAR hemisphere being visible (inside-out look).
    // Temporarily disable culling while drawing glTF primitives and restore.
    GLboolean cullWasOn = glIsEnabled(GL_CULL_FACE);
    if (cullWasOn) glDisable(GL_CULL_FACE);

    for (const auto& p : primitives) {
        glm::mat4 M = parentModel * p.bakedTransform;
        planetShader.setMat4("uModel", M);
        glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(M)));
        planetShader.setMat3("uNormalMat", nm);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, p.baseColorTex ? p.baseColorTex : whiteTex);

        glBindVertexArray(p.vao);
        if (p.indexed)
            glDrawElements(GL_TRIANGLES, p.indexCount, p.indexType, 0);
        else
            glDrawArrays(GL_TRIANGLES, 0, p.vertexCount);
    }

    if (cullWasOn) glEnable(GL_CULL_FACE);
}
