// Check production browser conversation filtering against the actual device JSON.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync('web/index.html','utf8'),source=html.split('<script>')[1].split('</script>')[0];
function element(){return{value:'',dataset:{},classList:{toggle(){}},getContext(){return{}}}}
const ids=new Map(),context={document:{documentElement:{outerHTML:html},getElementById(id){if(!ids.has(id))ids.set(id,element());return ids.get(id)},querySelectorAll(){return[]},createElement:element},TextDecoder,TextEncoder,Uint8Array,Uint8ClampedArray,DataView,Map,Set,JSON,Math,Number,Date,setTimeout,clearTimeout,URL:{},Blob,fetch(){throw Error('No network expected')}};
vm.createContext(context);vm.runInContext(source,context);
context.data=JSON.parse(fs.readFileSync(process.argv[2],'utf8'));context.node=process.argv[3];vm.runInContext('history=data;status={node};',context);
const matches=vm.runInContext('matches',context),other=vm.runInContext('other',context),rows=context.data;
const broadcasts=rows.filter(m=>m.destination==='ALL');assert(broadcasts.length>0,'Need real broadcast fixture');assert.deepStrictEqual(rows.filter(m=>matches(m,'ALL')),broadcasts);
const conversations=new Set(rows.map(other));let count=0;for(const id of conversations){const filtered=rows.filter(m=>matches(m,id));count+=filtered.length;if(id!=='ALL')assert(filtered.every(m=>m.destination!=='ALL'&&(m.outgoing?m.destination===id:m.source===id&&(m.destination===context.node||m.protocol===1))))}assert.strictEqual(count,rows.length,'Conversation filters must cover messages exactly once');
console.log(`PASS production web conversation filters with real device JSON: ${rows.length} messages, ${broadcasts.length} general, ${conversations.size} separate conversations`);
