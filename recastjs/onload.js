this['Recast'] = Module;

function recastInstallOffMeshHelper() {
    if (!Module.OffMeshLinkConfig || Module.OffMeshLinkConfig.prototype.GetInstance) {
        return;
    }

    function recastCopyToHeap(typedCtor, src) {
        var arr = src instanceof typedCtor ? src : new typedCtor(src);
        var ptr = Module._malloc(arr.byteLength);
        Module.HEAPU8.set(new Uint8Array(arr.buffer, arr.byteOffset, arr.byteLength), ptr);
        return ptr;
    }

    Module.OffMeshLinkConfig.prototype.GetInstance = function(verts, rad, flags, areas, dir, userID, count) {
        var cfg = new Module.OffMeshLinkConfig();
        cfg.offMeshConVerts = recastCopyToHeap(Float32Array, verts);
        cfg.offMeshConRad = recastCopyToHeap(Float32Array, rad);
        cfg.offMeshConFlags = recastCopyToHeap(Uint16Array, flags);
        cfg.offMeshConAreas = recastCopyToHeap(Uint8Array, areas);
        cfg.offMeshConDir = recastCopyToHeap(Uint8Array, dir);
        cfg.offMeshConUserID = recastCopyToHeap(Uint32Array, userID);
        cfg.offMeshConCount = count | 0;
        return cfg;
    };
}

recastInstallOffMeshHelper();
var previousInit = Module.onRuntimeInitialized;
Module.onRuntimeInitialized = function() {
    recastInstallOffMeshHelper();
    if (typeof previousInit === 'function') {
        previousInit();
    }
};
