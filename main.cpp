#include <cstdio>
#include <memory>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengles.h>

#include <emscripten.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>

#include "images/tiles/dark_grass.h"
#include "images/tiles/dry_grass.h"

#include "images/awesome_face.h"
#include "images/brick_wall.h"
#include "images/incredulous_face.h"
#include "images/shocked_face.h"

#include "box.hpp"
#include "box_data.hpp"
#include "camera.hpp"
#include "draw_instanced_no_texture.hpp"
#include "draw_instanced_with_texture.hpp"
#include "grid_square.hpp"
#include "square.hpp"
#include "texture.hpp"

namespace{

    /*! Helper
     *! Builds the vertices for the map grid
     */
    std::vector<calc::mat4f> build_grid(int width, int length)
    {
        static const float xdim = 1.0;
        static const float ydim = 1.0;

        calc::mat4f model = calc::mat4f::identity();
        float& x = model[3][0];
        float& y = model[3][1];

        const int xinst = std::ceil(width / 2.0 / xdim);
        const int yinst = std::ceil(length / 2.0 / ydim);

        const int size = xinst * yinst * 4;

        std::vector<calc::mat4f> grid;
        grid.resize(size);
        calc::mat4f* ptr = &grid[0];

        for (int i = -xinst; i != xinst; ++i)
        {
            for (int j = -yinst; j != yinst; ++j)
            {
                x = i * xdim + 0.5;
                y = j * ydim + 0.5;
                *ptr++ = model;
            }
        }

        return grid;
    }

    /*! Helper
     *! Build the vertices for the map wall
     */
    std::vector<calc::mat4f> build_wall(int width, int length)
    {
        std::vector<calc::mat4f> wall;

        // West wall
        for (int i = 1 - length / 2 / 3; i != length / 2 / 3; ++i)
        {
            calc::mat4f mat = calc::mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = width / 2 - 1;
            mat[1][3] = i * 3;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // East wall
        for (int i = 1 - length / 2 / 3; i != length / 2 / 3; ++i)
        {
            calc::mat4f mat = calc::mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = 1 - width / 2;
            mat[1][3] = i * 3;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // North wall
        for (int i = 1 - width / 2 / 3; i != width / 2 / 3; ++i)
        {
            calc::mat4f mat = calc::mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = i * 3;
            mat[1][3] = length / 2 - 1;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        // South wall
        for (int i = 1 - width / 2 / 3; i != width / 2 / 3; ++i)
        {
            calc::mat4f mat = calc::mat4f::identity();
            mat[0][0] = 3;
            mat[1][1] = 3;

            mat[0][3] = i * 3;
            mat[1][3] = 1 - length / 2;
            mat[2][3] = -1.0;
            wall.push_back(calc::transpose(mat));
        }

        return wall;
    }

    /*! Helper
     *! Converts matrix to float data
     */
    inline std::vector<float> copy_matrix_data(const std::vector<calc::mat4f>& mats)
    {
        std::vector<float> values(mats.size() * 16);
        for (::size_t i = 0; i != mats.size(); ++i)
            ::memcpy(&values[i * 16], calc::data(mats[i]), sizeof(calc::mat4f));
        return values;
    }
}

namespace {

    /*! Class Runner
     *! Encapsulates the main loop
     */
    class Runner {
    public:

        /*! ctor.
         */
        Runner(SDL_Window* window, Camera* camera) : window_(window)
                                                   , camera_(camera) {
            static const unsigned width = 30;
            static const unsigned height = 30;

            // Load boxes
            unsigned boxTAO1[] = {
                render::load_texture_from_data(brick_wall_png, brick_wall_png_len, false),
                render::load_texture_from_data(awesome_face_png, awesome_face_png_len, true)
            };

            unsigned boxTAO2[] = {
                boxTAO1[0],
                render::load_texture_from_data(shocked_face_png, shocked_face_png_len, true)
            };

            unsigned boxTAO3[] = {
                boxTAO1[0],
                render::load_texture_from_data(incredulous_face_png, incredulous_face_png_len, true)
            };

            unsigned wallTAO[] = {
                boxTAO1[0],
                boxTAO1[0],
            };

            textureHandles_.push_back(render::load_texture_from_data(awesome_face_png,
                                                                     awesome_face_png_len,
                                                                     true,
                                                                     false));
            textureHandles_.push_back(render::load_texture_from_data(shocked_face_png,
                                                                     shocked_face_png_len,
                                                                     true,
                                                                     false));
            textureHandles_.push_back(render::load_texture_from_data(incredulous_face_png,
                                                                     incredulous_face_png_len,
                                                                     true,
                                                                     false));

            ballObject_[0] = std::make_shared<render::Box>(boxTAO1, (sizeof(boxTAO1) / sizeof(unsigned)), 1);
            ballObject_[0]->push_back(calc::mat4f::identity());

            ballObject_[1] = std::make_shared<render::Box>(boxTAO2, (sizeof(boxTAO2) / sizeof(unsigned)), 1);
            ballObject_[1]->push_back(calc::mat4f::identity());

            ballObject_[2] = std::make_shared<render::Box>(boxTAO3, (sizeof(boxTAO3) / sizeof(unsigned)), 1);
            ballObject_[2]->push_back(calc::mat4f::identity());

            // Load map...
            float cageWidth = width + (width % 2);
            cageWidth_ = cageWidth;

            float cageLength = height + (height % 2);
            cageLength_ = cageLength;

            float gridWidth = 2 * cageWidth;
            float gridLength = 2 * cageLength;

            // Load grid tiles
            std::vector<float> grid = copy_matrix_data(build_grid(gridWidth, gridLength));

            gridTile_ = std::make_shared<render::GridSquare>(gridWidth * gridLength);
            gridTile_->reset(grid.data(), (grid.size() / 16));

            // Load wall
            std::vector<float> wall = copy_matrix_data(build_wall(cageWidth, cageLength));

            wallObject_ = std::make_shared<render::Box>(wallTAO,
                                                        sizeof(wallTAO) / sizeof(unsigned),
                                                        cageWidth * cageLength);
            wallObject_->reset(wall.data(), (wall.size() / 16));

            // Load dry grass tiles...
            unsigned dryGrassTextureTAO = render::load_texture_from_data(dry_grass_png, dry_grass_png_len, false);
            unsigned dryGrassTileTAO[] = {
                dryGrassTextureTAO,
                dryGrassTextureTAO
            };

            dryGrassTile_ = std::make_shared<render::Square>(dryGrassTileTAO,
                                                             sizeof(dryGrassTileTAO) / sizeof(unsigned),
                                                             gridWidth * gridLength);

            int gridMaxLength = gridLength / 2;
            int gridMinLength = -gridMaxLength;

            int gridMaxWidth = gridWidth / 2;
            int gridMinWidth = -gridMaxWidth;

            int cageMaxLength = cageLength / 2;
            int cageMinLength = -cageMaxLength;

            int cageMaxWidth = cageWidth / 2;
            int cageMinWidth = -cageMaxWidth;

            // Load dry grass coordinates
            calc::mat4f mat = calc::mat4f::identity();

            // Top field
            for (int i = cageMaxLength; i <= gridMaxLength; ++i)
            {
                for (int j = gridMinWidth; j <= gridMaxWidth; ++j)
                {
                    mat[3][0] = j;
                    mat[3][1] = i;
                    dryGrassTile_->push_back(mat);
                }
            }

            // Right field
            for (int i = cageMinLength; i <= cageMaxLength; ++i)
            {
                for (int j = gridMinWidth; j <= cageMinWidth + 1; ++j)
                {
                    mat[3][0] = j;
                    mat[3][1] = i;
                    dryGrassTile_->push_back(mat);
                }
            }

            // Left field
            for (int i = cageMinLength; i <= cageMaxLength; ++i)
            {
                for (int j = cageMaxWidth - 1; j <= gridMaxWidth; ++j)
                {
                    mat[3][0] = j;
                    mat[3][1] = i;
                    dryGrassTile_->push_back(mat);
                }
            }

            // Bottom field
            for (int i = gridMinLength; i <= cageMinLength; ++i)
            {
                for (int j = gridMinWidth; j <= gridMaxWidth; ++j)
                {
                    mat[3][0] = j;
                    mat[3][1] = i;
                    dryGrassTile_->push_back(mat);
                }
            }

            // Load fresh grass tiles...
            unsigned grassTextureTAO = render::load_texture_from_data(dark_grass_png, dark_grass_png_len, false);
            unsigned grassTileTAO[] = {
                grassTextureTAO,
                grassTextureTAO
            };

            grassTile_ = std::make_shared<render::Square>(grassTileTAO,
                                                          sizeof(grassTileTAO) / sizeof(unsigned),
                                                          cageWidth * cageLength);

            const int wallThickness = 2;

            // Load fresh grass coordinates
            for (int i = cageMinLength + wallThickness; i <= cageMaxLength - wallThickness; ++i)
            {
                for (int j = cageMinWidth + wallThickness; j <= cageMaxWidth - wallThickness; ++j)
                {
                    grassTile_->push_back(calc::transpose([i, j]() {

                        calc::mat4f mat = calc::mat4f::identity();
                        mat[0][3] = j;
                        mat[1][3] = i;
                        mat[2][3] = 0;
                        return mat;
                    }()));
                }
            }
        }

        /*! Run loop
         */
        void run() {

            //Handle events on queue
            SDL_Event e;
            while (SDL_PollEvent(&e) != 0)
            {
                if (e.type == SDL_QUIT) {
                    return;
                }

                switch (e.type)
                {
                    case SDL_WINDOWEVENT:
                    {
                        on_window_event(e);
                        break;
                    }

                    // Handle keypress with current mouse position
                    case SDL_TEXTINPUT:
                    {
                        on_text_input(e);
                        break;
                    }
                }
            }

            render();
        }

    private:

        /*! Helper
         *! Evt. handler
         */
        void on_window_event(const SDL_Event& e) {

            if ((e.window).event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                unsigned screenWidth = e.window.data1;
                unsigned screenHeight = e.window.data2;
                glViewport(0, 0, screenWidth, screenHeight);

                // Update camera
                Camera& refcamera = *camera_;
                refcamera.resize(screenWidth, screenHeight);
                refcamera.update();
            }
        }

        /*! Helper
         *! Evt. handler
         */
        void on_text_input(const SDL_Event&) {}

        /*! Helper
         *! Renders the scene
         */
        void render() {
#if 0
            glClearColor(backgroundColor[0],
                         backgroundColor[1],
                         backgroundColor[2],
                         1.0);
#endif
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const calc::mat4f& lookAt     = camera_->get_device_look_at();
            const calc::mat4f& projection = camera_->get_device_projection();
#if 0
            // Maybe draw the grid
            if (enableGrid)
#endif
            {
                gridDraw_.use();
#if 0
                gridDraw_.set_color(calc::vec4f(gridColor[0],
                                                gridColor[1],
                                                gridColor[2],
                                                1.0));
#endif
                gridDraw_.set_scene(lookAt, projection);
                gridTile_->draw();
            }

            // Draw the wall
            mainDraw_.use();
            mainDraw_.set_scene(lookAt, projection);
            wallObject_->draw();

            // Draw the grass inside the cage
            grassTile_->draw();
            // Draw the grass outside the cage
            dryGrassTile_->draw();

            // Draw the box
            calc::vec3f& direction = ballData_.direction;
            calc::vec3f& speed = ballData_.speed;
            calc::mat4f& translation = ballData_.translation;

            translation[2][3] = -1.0;
            float x = (translation[0][3] += speed[0] * direction[0]);
            float y = (translation[1][3] += speed[1] * direction[1]);

            // Bounce back on wall hit
            float hitOffset = 3.0;
            if (x < +hitOffset - (cageWidth_ / 2) ||
                x > -hitOffset + (cageWidth_ / 2)) {
                direction[0] *= -1;
            }

            if (y < +hitOffset - (cageLength_ / 2) ||
                y > -hitOffset + (cageLength_ / 2)) {
                direction[1] *= -1;
            }

            const calc::vec3f turnRate = ballData_.turnRate * calc::radians(SDL_GetTicks() / 10.0);
            const calc::mat4f boxMat = calc::transpose(translation
                                                       * calc::rotate_4x(turnRate[0])
                                                       * calc::rotate_4y(turnRate[1])
                                                       * calc::rotate_4z(turnRate[2]));
            // Do the draw call
            render::Box& refobject = *ballObject_[ballData_.selectedSkin];
            refobject.modify(calc::data(boxMat), 0);
            refobject.draw();
            // Update screen & return
            SDL_GL_SwapWindow(window_);
        }

        // Points to main SDL window
        SDL_Window* window_;

        // Camera / viewer
        Camera* camera_;
        // Contains ball position and rotation information
        BallData ballData_;

        // Program, uses instancing;
        // called to draw grid squares
        DrawInstancedNoTexture gridDraw_;
        // Program, uses instancing;
        // called to draw all textured objects
        DrawInstancedWithTexture mainDraw_;

        // Map item
        std::shared_ptr<render::Square>     grassTile_;
        // Map item
        std::shared_ptr<render::Square>     dryGrassTile_;
        // Map item
        std::shared_ptr<render::GridSquare> gridTile_;
        // Map item
        std::shared_ptr<render::Box>        ballObject_[3];
        // Map item
        std::shared_ptr<render::Box>        wallObject_;

        // Box skins
        std::vector<unsigned> textureHandles_;

        // Dimension
        float cageWidth_;
        // Dimension
        float cageLength_;
    };
}


void run(void* arg)
{
    Runner* runner = reinterpret_cast<Runner*>(arg);
    runner->run();
}

/*! Entry point
 */
int main(void)
{
    static const int screenWidth = 1000;
    static const int screenHeight = 1000;

    // Init SDL and OpenGL
    SDL_Window* window;
    SDL_Renderer* renderer = nullptr;
    SDL_CreateWindowAndRenderer(screenWidth, screenHeight, SDL_WINDOW_OPENGL, &window, &renderer);
    if (window == nullptr)
        return (printf("SDL window could not be created! SDL Error: %s\n", SDL_GetError()), 1);

    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, screenWidth, screenHeight);

    // Init camera defaults
    static float xPos = 0;
    static float yPos = 0;
    static float zPos = -20.0;
    static float fov  = 12.5;
    static float zFar = 1000.0;
    static float zNear = 0.01;

    // Init. Camera
    std::shared_ptr<Camera> camera(new Camera(calc::vec3f(xPos, yPos, zPos), fov, zFar, zNear, screenWidth, screenHeight));

    // Enter run loop
    Runner runner(window, camera.get());
    emscripten_set_main_loop_arg(run, &runner, 0, 1);
    return 0;
}
