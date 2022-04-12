
#if 0
internal ps_entity
CreateTerrainEntity(entt::registry& registry)
{
    ps_entity ret = registry.create();

    // create a terrian of 30 x 33 cells, to fit a pixel size of 64 x 32 on a 1080p screen
    // Each cell will be 1 square meter for sizing purposes, which sets the unit tiling...
    
    // NOTE(james): The odd number allows us to have a "center" tile, if that matters...
    const int max_x_tiles = 31; // 1984 pixels
    const int max_y_tiles = 35; // 1120 pixels

    // not technically half since some of the tiles will be partially offscreen
    const int half_max_x_tiles = max_x_tiles / 2;
    const int half_max_y_tiles = max_y_tiles / 2;

    f32 x_offset = -(half_max_x_tiles + 0.5f);
    f32 y_offset = -(half_max_y_tiles + 0.5f);

    f32 x_intervals[max_x_tiles+1];
    f32 y_intervals[max_y_tiles+1];

    for(int i = 0; i < max_x_tiles; ++i)
    {
        x_intervals[i] = x_offset + (f32)i;
    }

    for(int i = 0; i < max_y_tiles; ++i)
    {
        y_intervals[i] = y_offset + (f32)i;
    }

    u32 indices[max_x_tiles * max_y_tiles * 6] = {};
    render_vertex vertices[max_x_tiles * max_y_tiles * 4] = {};

    for(int y = 0; y < max_y_tiles; ++y)
    {
        for(int x = 0; x < max_x_tiles; ++x)
        {
            int i_idx = (y * max_x_tiles + x) * 6;
            int v_idx = (y * max_x_tiles + x) * 4;

            // TODO(james): allow y position to be changed?
            // NOTE(james): y_interval is actually the z-plane since y is up/down
            vertices[v_idx + 0] = {{x_intervals[x]  ,0.0f,y_intervals[y]  }, {0.0f,1.0f,0.0f}, {0.0f,1.0f}};
            vertices[v_idx + 1] = {{x_intervals[x]  ,0.0f,y_intervals[y+1]}, {0.0f,1.0f,0.0f}, {0.0f,0.0f}};
            vertices[v_idx + 2] = {{x_intervals[x+1],0.0f,y_intervals[y+1]}, {0.0f,1.0f,0.0f}, {1.0f,0.0f}};
            vertices[v_idx + 3] = {{x_intervals[x+1],0.0f,y_intervals[y]  }, {0.0f,1.0f,0.0f}, {1.0f,1.0f}};

            indices[i_idx + 0] = v_idx + 0; 
            indices[i_idx + 1] = v_idx + 2; 
            indices[i_idx + 2] = v_idx + 1;
            indices[i_idx + 3] = v_idx + 0; 
            indices[i_idx + 4] = v_idx + 3; 
            indices[i_idx + 5] = v_idx + 2;
        }
    }

    // TODO(james): Allocate vertex & index buffers on GPU for rendering
    

    return ret;
}

#endif