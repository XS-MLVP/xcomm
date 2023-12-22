export function xsp_run(fc){
// Load js
requirejs(["./jsxspcomm.js", "./emnapi.min.js"], function(_, napi){
    console.log("load jsxspcomm.js emnapi.min.js complete")
    Promise.all([xspcomm()]).then(([A]) => {
        var xsp = A.emnapiInit({ context: napi.getDefaultContext()})
        // Override XData
        class XData extends xsp.XData{
            constructor(...args){
                super(...args)
            }
            get value(){
                if(this.W() <= 64){
                    return this.AsInt64()
                }
                return this.GetVU8()
            }
            set value(v){
                if (typeof v === 'bigint') {
                    return this.SetVU8(v)
                }else{
                    return this.Set(v);
                }
            }
        }
        xsp.XData = XData

        // Override XClock
        class XClock extends xsp.XClock{
            constructor(...args){
                super(...args)
                this.event = null
                this.resolve = null
                this.reject = null
                this.resolve_tmp = null
                this.reject_tmp = null
                this.corutines = []
                this.conditions = []
                this._init_event()
            }
            get clk(){
                return super.GetClk()
            }
            _init_event(){
                this.event = new Promise((resolve, reject) => {
                    this.resolve = resolve
                    this.reject = reject
                    this.corutines = []
                })
            }
            async _resolve_event(){
                this.event = new Promise((resolve, reject) => {
                    this.resolve_tmp = resolve
                    this.reject_tmp = reject
                })
                this.resolve(this.clk)
                await Promise.all(this.corutines)
                this.reject = this.reject_tmp
                this.resolve = this.resolve_tmp
                this.corutines = []    
            }
            async _resolve_condition(){
                if(this.conditions.length == 0){
                    return
                }
                let wt_list = []
                let cond = this.conditions.shift()
                let [cond_func, resolve, reject, cd] = cond
                if(cond_func()){
                    resolve()
                    wt_list.push(cd)
                }else{
                    this.conditions.push(cond)
                }
                return Promise.all(wt_list)
            }
            _get_event(){
                let af = async()=>{await this.event}
                let cr = af();
                this.corutines.push(cr)
                return cr
            }
            // async functions
            async AStep(cycle = 1){
                for(var i = 0; i < cycle - 1; i++){
                    await this._get_event()
                }
                return this._get_event()
            }
            async ACondition(cond){
                let cd;
                cd = new Promise((resolve, reject) => {
                    this.conditions.push([cond, resolve, reject, cd])
                })
                return cd
            }
            async ANext(){
                return this.AStep(1)
            }
            async RunStep(cycle = 1){
                for(var i = 0; i < cycle; i++){
                    super.Step(1)
                    await this._resolve_event()
                    await this._resolve_condition()
                }
            }
        }
        xsp.XClock = XClock

        // call test bench
        fc(xsp)
    })
})
}
