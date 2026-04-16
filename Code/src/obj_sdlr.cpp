#include "obj_sdlr.h"

using namespace CRF;

ObjSdlr::ObjSdlr(){
    
}

bool ObjSdlr::loadObj(const std::string& path, std::vector<Vertex>& outVertices){
    std::ifstream file(path);
    if(!file.is_open()){
        std::cerr << "Failed to open OBJ file\n";
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec2> texcoords;
    std::vector<Vec3> normals;
    std::string line;

    while(std::getline(file, line)){
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if(prefix == "v"){              // vertex position
            Vec3 v;
            iss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if(prefix == "vt"){        // vertex texture coordinates
            Vec2 vt;
            iss >> vt.x >> vt.y;
            texcoords.push_back(vt);
        }
        else if(prefix == "vn"){        // vertex normal coordinates
            Vec3 vn;
            iss >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if(prefix == "f"){         // vertex face
            std::vector<FaceIndex> face;
            std::string token;

            while (iss >> token) {
                face.push_back(parseFaceVertex(token));
            }

            triangulateFaces(face, positions, texcoords, normals, outVertices);
        }
    }

    return true;
}

void ObjSdlr::triangulateFaces(
    const std::vector<FaceIndex>& face,
    const std::vector<Vec3>& positions, const std::vector<Vec2>& texcoords, const std::vector<Vec3>& normals,
    std::vector<Vertex>& outVertices
){
    for(int i = 0; i < (int)face.size() - 1; i++){
        FaceIndex a = face[0];
        FaceIndex b = face[i];
        FaceIndex c = face[i + 1];

        auto buildVertex = [&](FaceIndex idx) -> Vertex{
            Vertex v{};

            v.position = positions[idx.v - 1];

            if (idx.vt > 0){
                v.texcoord = texcoords[idx.vt - 1];
            }
            if (idx.vn > 0){
                v.normal = normals[idx.vn - 1];
            }

            return v;
        };

        outVertices.push_back(buildVertex(a));
        outVertices.push_back(buildVertex(b));
        outVertices.push_back(buildVertex(c));
    }
}

FaceIndex ObjSdlr::parseFaceVertex(const std::string& token){
    FaceIndex idx;
    std::stringstream ss(token);
    std::string temp;

    // vertex
    std::getline(ss, temp, '/');
    idx.v = std::stoi(temp);

    // vertex texture
    if(std::getline(ss, temp, '/')){
        if(!temp.empty()) idx.vt = stoi(temp);
    }

    // vertex normal
    if(std::getline(ss, temp, '/')){
        if(!temp.empty()) idx.vn = stoi(temp);
    } 

    return idx;
}
