
internal Entity
CreateTerrainEntity(World& world)
{
    
    // TODO(james): Allocate vertex & index buffers on GPU for rendering
    

    // TODO(james): Create a more extensible way to reference terrain since each region will have one
    return world.entity("terrain");
}
