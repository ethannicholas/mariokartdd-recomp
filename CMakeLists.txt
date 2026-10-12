cmake_minimum_required(VERSION 3.20)
project(mkdd C CXX)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Everything that is not specific to Mario Kart: Double Dash!! lives in gcn-recomp (a
# submodule): the recompiler, the GameCube runtime, the renderer and the frontends. See its
# README.
add_subdirectory(gcn-recomp)

gcn_add_game(
  NAME mkdd
  TITLE "Mario Kart: Double Dash!!"
  LABEL "Double Dash Recomp"
  DISC_ID GM4E01
  DOL_SHA1 e389e50357d4a5076ab3d38672204658f411123d
  SYMBOLS ${CMAKE_SOURCE_DIR}/analysis/symbols.txt
  TABLES ${CMAKE_SOURCE_DIR}/recomp
  FPS 60)
