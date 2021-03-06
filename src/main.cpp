//
//  main.cpp
//  FCE-To-OBJ
//
//  Created by Amrik Sadhra on 27/01/2015.
//
#include <cstdlib>
#include <string>
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <btBulletDynamicsCommon.h>
#include <examples/opengl3_example/imgui_impl_glfw_gl3.h>
#include <set>

#define TINYOBJLOADER_IMPLEMENTATION
// Source
#include "Scene/Camera.h"
#include "nfs_loader.h"
#include "trk_loader.h"
#include "Shaders/TrackShader.h"
#include "Shaders/CarShader.h"
#include "Util/Assert.h"

GLFWwindow *window;

using namespace ImGui;

class BulletDebugDrawer_DeprecatedOpenGL : public btIDebugDraw {
public:
    void SetMatrices(glm::mat4 pViewMatrix, glm::mat4 pProjectionMatrix) {
        glUseProgram(0); // Use Fixed-function pipeline (no shaders)
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(&pViewMatrix[0][0]);
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf(&pProjectionMatrix[0][0]);
    }

    virtual void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) {
        glColor3f(color.x(), color.y(), color.z());
        glBegin(GL_LINES);
        glVertex3f(from.x(), from.y(), from.z());
        glVertex3f(to.x(), to.y(), to.z());
        glEnd();
    }

    virtual void drawContactPoint(const btVector3 &, const btVector3 &, btScalar, int, const btVector3 &) {}

    virtual void reportErrorWarning(const char *) {}

    virtual void draw3dText(const btVector3 &, const char *) {}

    virtual void setDebugMode(int p) {
        m = p;
    }

    int getDebugMode(void) const { return 3; }

    int m;
};

bool init_opengl() {
    // Initialise GLFW
    ASSERT(glfwInit(), "GLFW Init failed.\n");

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Appease the OSX Gods
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    window = glfwCreateWindow(1024, 768, "OpenNFS3", nullptr, nullptr);

    if (window == nullptr) {
        fprintf(stderr, "Failed to create a GLFW window.\n");
        getchar();
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Needed for core profile

    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return false;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    // Set the mouse at the center of the screen
    glfwPollEvents();
    glfwSetCursorPos(window, 1024 / 2, 768 / 2);

    // Dark blue background
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

    // Cull triangles which normal is not towards the camera
    /*glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
    glEnable(GL_BACK);*/
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void newFrame(bool &window_active) {
    glfwPollEvents();
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Detect a click on the 3D Window by detecting a click that isn't on ImGui
    window_active = window_active ? window_active : (
            (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) &&
            (!ImGui::GetIO().WantCaptureMouse));
    if (!window_active) {
        ImGui::GetIO().MouseDrawCursor = false;
    }
    ImGui_ImplGlfwGL3_NewFrame();
}

int main(int argc, const char *argv[]) {
    std::cout << "----------- OpenNFS3 v0.01 -----------" << std::endl;
    ASSERT(init_opengl(), "OpenGL init failed.");

    NFS_Loader nfs_loader("../resources/car.viv");
    //Load Car data from unpacked NFS files
    std::vector<Car> cars = nfs_loader.getMeshes();
    cars[0].enable();
    //Load Track Data
    trk_loader trkLoader("../resources/TRK006/TR06.frd");
    std::map<short, GLuint> gl_id_map = trkLoader.getTextureGLMap();
    std::vector<TrackBlock> track_blocks = trkLoader.getTrackBlocks();
    std::vector<Light> track_lights = trkLoader.getLights();
    // TODO: Reference COLs to track blocks
    //std::vector<Track> col_models = trkLoader.getCOLModels();
    //cars.insert(cars.end(), col_models.begin(), col_models.end());

    /*------- BULLET --------*/
    btBroadphaseInterface *broadphase = new btDbvtBroadphase();
    // Set up the collision configuration and dispatcher
    btDefaultCollisionConfiguration *collisionConfiguration = new btDefaultCollisionConfiguration();
    auto *dispatcher = new btCollisionDispatcher(collisionConfiguration);
    // The actual physics solver
    auto *solver = new btSequentialImpulseConstraintSolver;
    // The world.
    auto *dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));
    BulletDebugDrawer_DeprecatedOpenGL mydebugdrawer;
    dynamicsWorld->setDebugDrawer(&mydebugdrawer);

    /*------- ImGui -------*/
    ImGui::CreateContext();
    ImGui_ImplGlfwGL3_Init(window, true);
    // Setup style
    ImGui::StyleColorsDark();

    // Create and compile our GLSL programs from the shaders
    TrackShader trackShader;
    CarShader carShader;

    // Data used for culling
    Camera mainCamera(glm::vec3(-31,0.07,-5), 45.0f);
    std::vector<TrackBlock> activeTrackBlocks;
    glm::vec3 oldWorldPosition(0, 0, 0);
    int closestBlockID = 0;

    /*------- MODELS --------*/
    // Gen VBOs, add to Bullet Physics
    for (auto &car : cars) {
        if (!car.genBuffers()) {
            return -1;
        }
        dynamicsWorld->addRigidBody(car.rigidBody);
    }
    for (auto &track_block : track_blocks) {
        for (auto &track_block_model : track_block.models) {
            if (!track_block_model.genBuffers()) {
                return -1;
            }
            dynamicsWorld->addRigidBody(track_block_model.rigidBody);
        }
    }

    /*------- UI -------*/
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    ImVec4 car_color = ImVec4(0.0f,0.0f,0.0f, 1.00f);
    ImVec4 test_light_color = ImVec4(1.0f,1.0f,1.0f, 1.00f);
    float specReflectivity = 1;
    float specDamper = 10;
    int blockDrawDistance = 15;
    bool window_active = true;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        newFrame(window_active);
        // Compute the MVP matrix from keyboard and mouse input
        mainCamera.computeMatricesFromInputs(window_active, ImGui::GetIO());
        glm::mat4 ProjectionMatrix = mainCamera.ProjectionMatrix;
        glm::mat4 ViewMatrix = mainCamera.ViewMatrix;
        glm::vec3 worldPosition = mainCamera.position;
        mydebugdrawer.SetMatrices(ViewMatrix, ProjectionMatrix);

        // If camera moved
        if ((oldWorldPosition.x != worldPosition.x) && (oldWorldPosition.z != worldPosition.z)) {
            float lowestDistanceSqr = FLT_MAX;
            //Primitive Draw distance
            for (auto &track_block : track_blocks) {
                glm::quat orientation = glm::normalize(glm::quat(glm::vec3(-SIMD_PI/2,0,0)));
                glm::vec3 position = orientation*glm::vec3(track_block.trk.ptCentre.x / 10,track_block.trk.ptCentre.y / 10,track_block.trk.ptCentre.z / 10);
                float distanceSqr = glm::length2(glm::distance(worldPosition, glm::vec3(position.x,position.y, position.z)));
                if (distanceSqr < lowestDistanceSqr) {
                    closestBlockID = track_block.block_id;
                    lowestDistanceSqr = distanceSqr;
                }
            }
            int frontBlock =
                    closestBlockID < track_blocks.size() - blockDrawDistance ? closestBlockID + blockDrawDistance
                                                                             : track_blocks.size();
            int backBlock = closestBlockID - blockDrawDistance > 0 ? closestBlockID - blockDrawDistance : 0;
            std::vector<TrackBlock>::const_iterator first = track_blocks.begin() + backBlock;
            std::vector<TrackBlock>::const_iterator last = track_blocks.begin() + frontBlock;
            activeTrackBlocks = std::vector<TrackBlock>(first, last);
            oldWorldPosition = worldPosition;
        }

        carShader.use();
        for (auto &car : cars) {
            car.update();
            carShader.loadMatrices(ProjectionMatrix, ViewMatrix, car.ModelMatrix);
            carShader.loadSpecular(specDamper, specReflectivity);
            carShader.loadCarColor(glm::vec3(car_color.x, car_color.y, car_color.z));
            carShader.loadLight(Light(worldPosition, glm::vec3(test_light_color.x, test_light_color.y, test_light_color.z)));
            carShader.loadCarTexture();
            car.render();
        }
        carShader.unbind();

        trackShader.use();
        for (auto &active_track_Block : activeTrackBlocks) {
            for (auto &track_block_model : active_track_Block.models) {
                track_block_model.update();
                glm::mat4 MVP = ProjectionMatrix * ViewMatrix * track_block_model.ModelMatrix;
                trackShader.loadMVPMatrix(MVP);
                trackShader.bindTrackTextures(track_block_model, gl_id_map);
                track_block_model.render();
            }
        }
        trackShader.unbind();

        float lightSize = 0.5;
        for(auto &light : track_lights){
            glm::quat orientation = glm::normalize(glm::quat(glm::vec3(-SIMD_PI/2,0,0)));
            glm::vec3 position_min = orientation*glm::vec3(light.position.x/10-lightSize,light.position.y/10-lightSize,light.position.z/10-lightSize);
            glm::vec3 position_max = orientation*glm::vec3(light.position.x/10+lightSize,light.position.y/10+lightSize,light.position.z/10+lightSize);
            btVector3 colour = btVector3(light.type/15, light.type/15,light.type/15);
            mydebugdrawer.drawBox(btVector3(position_min.x, position_min.y, position_min.z), btVector3(position_max.x, position_max.y, position_max.z),colour);
        }

        // Draw UI (Tactically)
        static float f = 0.0f;
        ImGui::Text("NFS3 Engine");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
        std::stringstream world_position_string;
        world_position_string << "X " << std::to_string(worldPosition.x) << " Y " << std::to_string(worldPosition.x)
                              << " Z " << std::to_string(worldPosition.z);
        ImGui::Text(world_position_string.str().c_str());
        ImGui::Text(("Block ID: " + std::to_string(closestBlockID)).c_str());
        if (ImGui::Button("Reset View")) {
            mainCamera.resetView();
        };
        ImGui::SliderInt("Draw Distance", &blockDrawDistance, 0, 200);
        ImGui::ColorEdit3("Clear Colour", (float *) &clear_color); // Edit 3 floats representing a color
        ImGui::ColorEdit3("Testing Light Colour", (float *) &test_light_color);
        ImGui::ColorEdit3("Car Colour", (float *) &car_color);
        ImGui::SliderFloat("Specular Damper", &specDamper, 0, 100);
        ImGui::SliderFloat("Specular Reflectivity", &specReflectivity, 0, 10);

        for (auto &mesh : cars) {
            ImGui::Checkbox((mesh.m_name + std::to_string(mesh.id)).c_str(), &mesh.enabled);      // Edit bools storing model draw state
        }
        if (ImGui::TreeNode("Track Blocks")) {
            for (auto &track_block : track_blocks) {
                if (ImGui::TreeNode((void *) track_block.block_id, "Track Block %d", track_block.block_id)) {
                    for (auto &block_model : track_block.models) {
                        if (ImGui::TreeNode((void *) block_model.id, "%s %d", block_model.m_name.c_str(), block_model.id)) {
                            ImGui::Checkbox("Enabled", &block_model.enabled);
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        // Rendering
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImGui::Render();
        ImGui_ImplGlfwGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup VBOs and shaders
    for (auto &car : cars) {
        car.destroy();
    }
    carShader.cleanup();
    trackShader.cleanup();
    ImGui_ImplGlfwGL3_Shutdown();
    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}
