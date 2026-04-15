#pragma once

#include "vertex.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace CRF{

    class ObjSdlr{
      public:
      ObjSdlr();
      bool loadObj(const std::string& path, std::vector<vertex>& outVertices);
      
      private:
      std::vector<vertex> vertices;
    };
}