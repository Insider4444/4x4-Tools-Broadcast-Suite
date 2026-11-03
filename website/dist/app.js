const content={
 presence:{tag:'EYE CONTACT + AUTO FRAME',symbol:'◎',title:"Be there. Even when you're reading.",description:"Redirect a single subject's gaze toward the camera and reframe the shot around them. Made for talking-head clips, interviews and presentations.",effects:['4x4 Eye Contact','4x4 Auto Frame']},
 background:{tag:'BACKGROUND + STUDIO LIGHT',symbol:'◐',title:'Your subject. Your setting.',description:'Remove or blur the background, then shape the light on your subject. Background Remove makes transparency: place your replacement on a lower video track or layer.',effects:['4x4 Background Remove','4x4 Background Blur','4x4 Studio Light']},
 voice:{tag:'SIX SPEECH PROCESSORS',symbol:'≋',title:'Give every word some room.',description:'Reduce noise and room echo, enhance a voice recording or restore narrowband speech. Start with Voice Clean for combined cleanup; choose Studio Voice HQ for offline renders.',effects:['Noise Clean','Room Echo Clean','Voice Clean','Studio Voice Preview','Studio Voice HQ','Voice Restore']},
 detail:{tag:'DENOISE + DETAIL RESTORE',symbol:'✧',title:'A cleaner picture. A careful finish.',description:'Use Preserve Fine Detail for gentler video denoising, or choose Heavy Noise for stronger cleanup. Detail Restore applies video super resolution at the current Adobe frame dimensions.',effects:['4x4 Video Denoise','4x4 Detail Restore']}
};
const tabs=[...document.querySelectorAll('[role=tab]')];
function select(tab,focus=false){
 const value=content[tab.dataset.effect];
 for(const t of tabs){const active=t===tab;t.setAttribute('aria-selected',String(active));t.tabIndex=active?0:-1}
 document.querySelector('#effect-panel').setAttribute('aria-labelledby',tab.id);
 for(const [id,key] of [['effect-tag','tag'],['effect-symbol','symbol'],['effect-title','title'],['effect-description','description']])document.getElementById(id).textContent=value[key];
 document.getElementById('effect-pills').replaceChildren(...value.effects.map(text=>{const span=document.createElement('span');span.textContent=text;return span}));
 if(focus)tab.focus();
}
tabs.forEach((tab,index)=>{
 tab.addEventListener('click',()=>select(tab));
 tab.addEventListener('keydown',event=>{let next;if(event.key==='ArrowRight'||event.key==='ArrowDown')next=(index+1)%tabs.length;else if(event.key==='ArrowLeft'||event.key==='ArrowUp')next=(index+tabs.length-1)%tabs.length;else if(event.key==='Home')next=0;else if(event.key==='End')next=tabs.length-1;if(next!==undefined){event.preventDefault();select(tabs[next],true)}});
});
const status=document.getElementById('download-status');
document.querySelectorAll('a.download').forEach(a=>a.addEventListener('click',()=>{status.hidden=false}));
status.querySelector('button').addEventListener('click',()=>{status.hidden=true});
