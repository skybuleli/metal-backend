namespace Ryujinx.Graphics.GAL
{
    public struct ShaderInfo
    {
        public int FragmentOutputMap { get; }
        public ResourceLayout ResourceLayout { get; }
        public ProgramPipelineState? State { get; }
        public int ComputeLocalSizeX { get; }
        public int ComputeLocalSizeY { get; }
        public int ComputeLocalSizeZ { get; }
        public bool FromCache { get; set; }

        public ShaderInfo(int fragmentOutputMap, ResourceLayout resourceLayout, ProgramPipelineState? state, bool fromCache = false)
            : this(fragmentOutputMap, resourceLayout, state, 0, 0, 0, fromCache)
        {
        }

        public ShaderInfo(
            int fragmentOutputMap,
            ResourceLayout resourceLayout,
            ProgramPipelineState? state,
            int computeLocalSizeX,
            int computeLocalSizeY,
            int computeLocalSizeZ,
            bool fromCache = false)
        {
            FragmentOutputMap = fragmentOutputMap;
            ResourceLayout = resourceLayout;
            State = state;
            ComputeLocalSizeX = computeLocalSizeX;
            ComputeLocalSizeY = computeLocalSizeY;
            ComputeLocalSizeZ = computeLocalSizeZ;
            FromCache = fromCache;
        }

        public ShaderInfo(int fragmentOutputMap, ResourceLayout resourceLayout, bool fromCache = false)
            : this(fragmentOutputMap, resourceLayout, default(ProgramPipelineState?), 0, 0, 0, fromCache)
        {
        }

        public ShaderInfo(
            int fragmentOutputMap,
            ResourceLayout resourceLayout,
            int computeLocalSizeX,
            int computeLocalSizeY,
            int computeLocalSizeZ,
            bool fromCache = false)
            : this(fragmentOutputMap, resourceLayout, default(ProgramPipelineState?), computeLocalSizeX, computeLocalSizeY, computeLocalSizeZ, fromCache)
        {
        }
    }
}
