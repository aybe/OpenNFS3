//
// Created by Amrik on 25/10/2017.
//

#include "Model.h"

#include <utility>

Model::Model(std::string name, int model_id, std::vector<glm::vec3> verts, std::vector<glm::vec2> uvs, std::vector<glm::vec3> norms,
             std::vector<unsigned int> indices, bool removeIndexing, std::vector<short> tex_ids) {
    m_name = std::move(name);
    id =  model_id;
    m_uvs = std::move(uvs);
    m_normals = std::move(norms);
    m_vertex_indices = std::move(indices);
    texture_ids = std::move(tex_ids);
    indexed = !removeIndexing;

    if (removeIndexing) {
        // Remove indexing
        for (unsigned int m_vertex_index : m_vertex_indices) {
            m_vertices.push_back(verts[m_vertex_index]);
        }
    } else {
        m_vertices = std::move(verts);
    }

    position = glm::vec3(0, 0, 0);
    orientation_vec = glm::vec3(0,0,0);
    orientation = glm::normalize(glm::quat(orientation_vec));
    //Generate Physics collision data
    motionstate = new btDefaultMotionState(btTransform(
            btQuaternion(orientation.x, orientation.y, orientation.z, orientation.w),
            btVector3(position.x, position.y, position.z)
    ));
    rigidBodyCI = btRigidBody::btRigidBodyConstructionInfo(
            0,                  // mass, in kg. 0 -> Static object, will never move.
            motionstate,
            genCollisionBox(m_vertices),  // collision shape of body
            btVector3(0, 0, 0)    // local inertia
    );
    rigidBody = new btRigidBody(rigidBodyCI);
    rigidBody->setUserPointer(this);
}

std::string Model::getName() {
    return m_name;
}


std::vector<glm::vec3> Model::getVertices() {
    return m_vertices;
}


std::vector<glm::vec2> Model::getUVs() {
    return m_uvs;
}


std::vector<glm::vec3> Model::getNormals() {
    return m_normals;
}


std::vector<unsigned int> Model::getIndices() {
    return m_vertex_indices;
}


void Model::update() {
    if (track){
        position = glm::vec3(0, 0, 0);
        orientation_vec = glm::vec3(-SIMD_PI/2,0,0);
    } else {
        if (m_name.find("wheel") != std::string::npos){
            
        }
        position = glm::vec3(-31,0.07,-5);
        orientation_vec = glm::vec3(0,0,0);
    }
    orientation = glm::normalize(glm::quat(orientation_vec));
    position = glm::vec3(position.x, position.y, position.z);
    RotationMatrix = glm::toMat4(orientation);
    TranslationMatrix = glm::translate(glm::mat4(1.0), position);
    ModelMatrix = TranslationMatrix * RotationMatrix;
}

void Model::enable() {
    enabled = true;
}


void Model::destroy() {
    glDeleteBuffers(1, &gl_buffers.vertexbuffer);
    glDeleteBuffers(1, &gl_buffers.uvbuffer);
    glDeleteBuffers(1, &gl_buffers.normalbuffer);
    glDeleteBuffers(1, &gl_buffers.shadingBuffer);
}

void Model::render() {
    if (!enabled)
        return;

    // 1st attribute buffer : Vertices
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffers.vertexbuffer);
    glVertexAttribPointer(
            0,                  // attribute
            3,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            (void *) 0            // array buffer offset
    );
    // 2nd attribute buffer : UVs
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffers.uvbuffer);
    glVertexAttribPointer(
            1,                                // attribute
            2,                                // size
            GL_FLOAT,                         // type
            GL_FALSE,                         // normalized?
            0,                                // stride
            (void *) 0                          // array buffer offset
    );
    // 3rd attribute buffer : Normals
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffers.normalbuffer);
    glVertexAttribPointer(
            2,                  // attribute
            3,                  // size
            GL_FLOAT,           // type
            GL_TRUE,           // normalized?
            0,                  // stride
            (void *) 0            // array buffer offset
    );
    // 4th attribute buffer : NFS3 Shading Data
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffers.shadingBuffer);
    glVertexAttribPointer(
            3,                  // attribute
            4,                  // size
            GL_FLOAT,           // type
            GL_FALSE,           // normalized?
            0,                  // stride
            (void *) 0            // array buffer offset
    );
    if(indexed){
        // Index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_buffers.elementbuffer);
        // Draw the triangles !
        glDrawElements(
                GL_TRIANGLES,      // mode
                m_vertex_indices.size(),    // count
                GL_UNSIGNED_INT,   // type
                (void *) 0           // element array buffer offset
        );
    } else {
        // Draw the triangles !
        glDrawArrays(GL_TRIANGLES, 0, m_vertices.size());
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);
    }
}

bool Model::genBuffers() {
    // Verts
    glGenBuffers(1, &gl_buffers.vertexbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffers.vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(glm::vec3), &m_vertices[0], GL_STATIC_DRAW);
    // UVs
    glGenBuffers(1, &gl_buffers.uvbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffers.uvbuffer);
    glBufferData(GL_ARRAY_BUFFER, m_uvs.size() * sizeof(glm::vec2), &m_uvs[0], GL_STATIC_DRAW);
    // Normals
    glGenBuffers(1, &gl_buffers.normalbuffer);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffers.normalbuffer);
    glBufferData(GL_ARRAY_BUFFER, m_normals.size() * sizeof(glm::vec3), &m_normals[0], GL_STATIC_DRAW);
    // NFS3 Shading Data
    glGenBuffers(1, &gl_buffers.shadingBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, gl_buffers.shadingBuffer);
    glBufferData(GL_ARRAY_BUFFER, m_shading_data.size() * sizeof(glm::vec4), &m_shading_data[0], GL_STATIC_DRAW);
    if (indexed){
        // Indices
        glGenBuffers(1, &gl_buffers.elementbuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_buffers.elementbuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_vertex_indices.size() * sizeof(unsigned int), &m_vertex_indices[0], GL_STATIC_DRAW);
    }
    return true;
}
