#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace CRF{
#pragma region structs
  struct Vec2{
    float x, y;
  };
  struct Vec3{
    float x, y, z;
  };
  struct Vertex{
    Vec3 position;
    Vec2 texcoord; 
    Vec3 normal;
  };

  struct FaceIndex{
    int v = 0;
    int vt = 0;
    int vn = 0;
  };
#pragma endregion

    // OBJ Snigedoodle Loader
    class ObjSdlr{
      public:
      ObjSdlr();
      bool loadObj(const std::string& path, std::vector<Vertex>& outVertices);
      FaceIndex parseFaceVertex(const std::string& token);
      void triangulateFaces(
        const std::vector<FaceIndex>& face,
        const std::vector<Vec3>& positions, const std::vector<Vec2>& texcoords, const std::vector<Vec3>& normals,
        std::vector<Vertex>& outVertices
      );
      
      private:
      std::vector<Vertex> vertices;
    };
}