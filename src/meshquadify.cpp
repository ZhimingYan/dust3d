#include <QDebug>
#include <unordered_set>
#include <vector>
#include "meshquadify.h"
#include "meshlite.h"

int meshQuadify(void *meshlite, int meshId, const std::set<std::pair<PositionMapKey, PositionMapKey>> &sharedQuadEdges,
    PositionMap<int> *positionMapForMakeKey)
{
    int vertexCount = meshlite_get_vertex_count(meshlite, meshId);
    float *vertexPositions = new float[vertexCount * 3];
    int vertexArrayLen = meshlite_get_vertex_position_array(meshlite, meshId, vertexPositions, vertexCount * 3);
    int offset = 0;
    Q_ASSERT(vertexArrayLen == vertexCount * 3);
    std::vector<PositionMapKey> positionKeyMap;
    for (int i = 0; i < vertexCount; i++) {
        float x = vertexPositions[offset + 0];
        float y = vertexPositions[offset + 1];
        float z = vertexPositions[offset + 2];
        positionKeyMap.push_back(positionMapForMakeKey->makeKey(x, y, z));
        offset += 3;
    }
    int faceCount = meshlite_get_face_count(meshlite, meshId);
    int *faceVertexNumAndIndices = new int[faceCount * MAX_VERTICES_PER_FACE];
    int filledLength = meshlite_get_face_index_array(meshlite, meshId, faceVertexNumAndIndices, faceCount * MAX_VERTICES_PER_FACE);
    int i = 0;
    std::vector<std::vector<int>> newFaceIndices;
    while (i < filledLength) {
        int num = faceVertexNumAndIndices[i++];
        Q_ASSERT(num > 0 && num <= MAX_VERTICES_PER_FACE);
        if (num < 3) {
            i += num;
            continue;
        }
        std::vector<int> indices;
        for (int j = 0; j < num; j++) {
            int index = faceVertexNumAndIndices[i++];
            Q_ASSERT(index >= 0 && index < vertexCount);
            indices.push_back(index);
        }
        newFaceIndices.push_back(indices);
    }
    int quadMesh = 0;
    std::map<std::pair<int, int>, std::pair<int, int>> triangleEdgeMap;
    for (int i = 0; i < (int)newFaceIndices.size(); i++) {
        const auto &faceIndices = newFaceIndices[i];
        if (faceIndices.size() == 3) {
            triangleEdgeMap[std::make_pair(faceIndices[0], faceIndices[1])] = std::make_pair(i, faceIndices[2]);
            triangleEdgeMap[std::make_pair(faceIndices[1], faceIndices[2])] = std::make_pair(i, faceIndices[0]);
            triangleEdgeMap[std::make_pair(faceIndices[2], faceIndices[0])] = std::make_pair(i, faceIndices[1]);
        }
    }
    std::unordered_set<int> unionedFaces;
    std::vector<std::vector<int>> newUnionedFaceIndices;
    for (const auto &edge: triangleEdgeMap) {
        if (unionedFaces.find(edge.second.first) != unionedFaces.end())
            continue;
        auto pair = std::make_pair(positionKeyMap[edge.first.first], positionKeyMap[edge.first.second]);
        if (sharedQuadEdges.find(pair) != sharedQuadEdges.end()) {
            auto oppositeEdge = triangleEdgeMap.find(std::make_pair(edge.first.second, edge.first.first));
            if (oppositeEdge == triangleEdgeMap.end()) {
                qDebug() << "Find opposite edge failed";
            } else {
                if (unionedFaces.find(oppositeEdge->second.first) == unionedFaces.end()) {
                    unionedFaces.insert(edge.second.first);
                    unionedFaces.insert(oppositeEdge->second.first);
                    std::vector<int> indices;
                    indices.push_back(edge.second.second);
                    indices.push_back(edge.first.first);
                    indices.push_back(oppositeEdge->second.second);
                    indices.push_back(edge.first.second);
                    newUnionedFaceIndices.push_back(indices);
                }
            }
        }
    }
    std::vector<int> newFaceVertexNumAndIndices;
    for (int i = 0; i < (int)newFaceIndices.size(); i++) {
        if (unionedFaces.find(i) == unionedFaces.end()) {
            const auto &faceIndices = newFaceIndices[i];
            newFaceVertexNumAndIndices.push_back(faceIndices.size());
            for (const auto &index: faceIndices) {
                newFaceVertexNumAndIndices.push_back(index);
            }
        }
    }
    for (const auto &faceIndices: newUnionedFaceIndices) {
        newFaceVertexNumAndIndices.push_back(faceIndices.size());
        for (const auto &index: faceIndices) {
            newFaceVertexNumAndIndices.push_back(index);
        }
    }
    quadMesh = meshlite_build(meshlite, vertexPositions, vertexCount, newFaceVertexNumAndIndices.data(), newFaceVertexNumAndIndices.size());
    delete[] faceVertexNumAndIndices;
    delete[] vertexPositions;
    return quadMesh;
}
