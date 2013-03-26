#include "gtest.h"
#include "core/formats/ply/PlyFile.h"
#include "core/formats/ply/PlyParseError.h"
#include "raytracer/primitives/Mesh.h"
#include "test/helpers/ContainerTestHelper.h"

#include <string>
#include <sstream>
#include <fstream>

using namespace std;

namespace PlyFileTest {
  using namespace ::testing;
  using namespace raytracer;
  
  TEST(PlyFile, ShouldParseMinimalFile) {
    istringstream stream("ply\n");
    PlyFile file(stream);
  }
  
  TEST(PlyFile, ShouldParseFormat) {
    istringstream stream("ply\nformat ascii 1.0\n");
    PlyFile file(stream);
  }
  
  TEST(PlyFile, ShouldIgnoreComments) {
    istringstream stream("ply\ncomment foo\n");
    PlyFile file(stream);
  }
  
  TEST(PlyFile, ShouldParseElement) {
    istringstream stream("ply\nelement vertex 5\n");
    PlyFile file(stream);
    ASSERT_EQ(1, file.elementCount());
  }
  
  TEST(PlyFile, ShouldParseProperties) {
    istringstream stream("ply\nelement vertex 5\nproperty float32 x\n");
    PlyFile file(stream);
    ASSERT_EQ(1u, file.elements().front().properties().size());
  }
  
  TEST(PlyFile, ShouldAssignPropertyToCurrentElement) {
    istringstream stream("ply\nelement foo 2\nelement vertex 5\nproperty float32 x\n");
    PlyFile file(stream);
    ASSERT_EQ(0u, file.elements().front().properties().size());
    ASSERT_EQ(1u, file.elements().back().properties().size());
  }
  
  TEST(PlyFile, ShouldThrowParseErrorIfUnknownTokenIsEncountered) {
    istringstream stream("ply\nfoobar\n");
    ASSERT_THROW(PlyFile file(stream), PlyParseError);
  }
  
  TEST(PlyFile, ShouldIgnoreEverythingAfterTheHeaderInTheFirstPass) {
    istringstream stream("ply\nend_header\nfoo bar\n");
    PlyFile file(stream);
  }
  
  TEST(PlyFile, ShouldWorkWithTypicalVertexProperties) {
    istringstream stream("ply\nelement vertex 1\nproperty float32 x\nproperty float32 y\n property float32 z\nend_header\n");
    PlyFile file(stream);
    ASSERT_EQ(3u, file.elements().front().properties().size());
  }
  
  TEST(PlyFile, ShouldWorkWithTypicalNormalProperties) {
    istringstream stream("ply\nelement vertex 1\nproperty float32 nx\nproperty float32 ny\n property float32 nz\nend_header\n");
    PlyFile file(stream);
    ASSERT_EQ(3u, file.elements().front().properties().size());
  }
  
  TEST(PlyFile, ShouldWorkWithTypicalFaceProperties) {
    istringstream stream("ply\nelement face 1\nproperty list uint8 int32 vertex_indices\nend_header\n");
    PlyFile file(stream);
    ASSERT_EQ(1u, file.elements().front().properties().size());
    ASSERT_TRUE(file.elements().front().properties().front().isList());
  }
  
  TEST(PlyFile, ShouldTreatVertexElementAsMeshVertex) {
    istringstream stream("ply\nelement vertex 1\nproperty float32 x\nproperty float32 y\n property float32 z\nend_header\n1 2 3\n");
    Mesh mesh;
    PlyFile file(stream, mesh);
    ASSERT_EQ(1u, mesh.vertices.size());
  }
  
  TEST(PlyFile, ShouldReadVertexPoints) {
    istringstream stream("ply\nelement vertex 1\nproperty float32 x\nproperty float32 y\n property float32 z\nend_header\n1 2 3\n");
    Mesh mesh;
    PlyFile file(stream, mesh);
    ASSERT_EQ(Vector3d(1, 2, 3), mesh.vertices.front().point);
  }
  
  TEST(PlyFile, ShouldReadVertexNormals) {
    istringstream stream("ply\nelement vertex 1\nproperty float32 nx\nproperty float32 ny\n property float32 nz\nend_header\n1 2 3\n");
    Mesh mesh;
    PlyFile file(stream, mesh);
    ASSERT_EQ(Vector3d(1, 2, 3), mesh.vertices.front().normal);
  }
  
  TEST(PlyFile, ShouldTreatFaceElementAsMeshFace) {
    istringstream stream("ply\nelement face 1\nproperty list uint8 int32 vertex_indices\nend_header\n3 1 2 3\n");
    Mesh mesh;
    PlyFile file(stream, mesh);
    ASSERT_EQ(1u, mesh.faces.size());
    ASSERT_EQ(3u, mesh.faces.front().size());
  }
  
  TEST(PlyFile, ShouldReadFaces) {
    istringstream stream("ply\nelement face 1\nproperty list uint8 int32 vertex_indices\nend_header\n3 1 2 3\n");
    Mesh mesh;
    PlyFile file(stream, mesh);
    ASSERT_CONTAINERS_EQ(makeStdVector(1, 2, 3), mesh.faces.front());
  }
  
  TEST(PlyFile, ShouldIgnoreOtherElements) {
    istringstream stream("ply\nelement foo 1\nproperty float32 bar\nend_header\n1\n");
    Mesh mesh;
    PlyFile file(stream, mesh);
    ASSERT_EQ(0u, mesh.vertices.size());
    ASSERT_EQ(0u, mesh.faces.size());
  }
  
  TEST(PlyFile, ShouldLoadPlyFile) {
    ifstream stream("test/fixtures/cube.ply");
    Mesh mesh;
    PlyFile file(stream, mesh);
    ASSERT_EQ(8u, mesh.vertices.size());
    ASSERT_EQ(6u, mesh.faces.size());
  }
}
