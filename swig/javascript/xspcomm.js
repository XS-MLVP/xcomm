export function xsp_run(fc){
// Load js
requirejs(["./jsxspcomm.js", "./emnapi.min.js"], function(_, napi){
    console.log("load jsxspcomm.js emnapi.min.js complete")
    Module.onRuntimeInitialized = function () {
        var xsp = Module.emnapiInit({ context: napi.getDefaultContext()})
        // Override XData
        class XData extends xsp.XData{
            constructor(...args){
                super(...args)
            }
            get value(){
                return this.AsInt64();
            }
            set value(v){
                return this.Set(v);
            }
        }
        xsp.XData = XData
        // OVerride XClock
        // ...

        // call test bench
        fc(xsp)
    }
})
}
