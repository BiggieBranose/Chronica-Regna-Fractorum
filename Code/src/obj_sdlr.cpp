#include "obj_sdlr.h"

using namespace CRF;

ObjSdlr::ObjSdlr(){
    
}

bool ObjSdlr::loadObj(const std::string& path, std::vector<vertex>& outVertices){
    std::ifstream file(path);
    if(!file.is_open()){
        std::cerr << "Failed to open OBJ file\n";
        return false;
    }

    std::vector<vec3> positions;
    std::vector<vec2> texcoords;
    std::vector<vec3> normals;
    std::string line;

    while(std::getline(file, line)){
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if(prefix == "v"){              // vertex position
            vec3 v;
            iss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if(prefix == "vt"){        // vertex texture coordinates
            vec2 vt;
            iss >> vt.x >> vt.y;
            texcoords.push_back(vt);
        }
        else if(prefix == "vn"){        // vertex normal coordinates
            vec3 vn;
            iss >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if(prefix == "f"){         // obj face
            std::string vertStr;

            // Assume 3 vertices for faces (OBJ faces are usually triangles)
            for(int i = 0; i < 3; i++){
                iss >> vertStr;

                std::istringstream vss(vertStr);
                std::string indexStr;

                int posIndex = 0, texIndex = 0, normIndex = 0;

                std::getline(vss, indexStr, '/');
                posIndex = std::stoi(indexStr);
                
                std::getline(vss, indexStr, '/');
                texIndex = std::stoi(indexStr);

                std::getline(vss, indexStr, '/');
                normIndex = std::stoi(indexStr);

                vertex vert;
                vert.position = positions[posIndex - 1];
                vert.texcoord = texcoords[texIndex - 1];
                vert.normal = normals[normIndex - 1];

                outVertices.push_back(vert);
            }
        }
    }

    return true;
}