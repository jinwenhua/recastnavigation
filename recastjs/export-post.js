if (typeof globalThis !== 'undefined') {
    globalThis.Recast = Recast;
}
if (typeof module === 'object' && module.exports) {
    module.exports = Recast;
}
