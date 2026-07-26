#include "core/web_page.h"

#include <cstring>

namespace vario {

// Page servie par le portail. R"=====(...)=====" pour ne pas avoir à échapper
// les guillemets du HTML. Rester sobre : le portail tourne sur un ESP32, la page
// doit être légère et fonctionner sur le navigateur d'un téléphone en plein
// soleil, hors ligne.
const char kConfigPageHtml[] = R"=====(<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Variomètre — réglages</title>
<style>
:root{color-scheme:light dark}
body{font-family:system-ui,sans-serif;margin:0;padding:1rem;max-width:32rem;
  margin-inline:auto;line-height:1.5}
h1{font-size:1.3rem;margin:.2rem 0 1rem}
.field{display:flex;align-items:center;gap:.6rem;padding:.5rem 0;
  border-bottom:1px solid #8883}
.field label{flex:1;font-weight:600}
.field .unit{opacity:.6;min-width:2.5rem}
input[type=number],select{width:6rem;padding:.3rem;font-size:1rem}
input[type=checkbox]{width:1.4rem;height:1.4rem}
button{margin-top:1rem;padding:.7rem 1.2rem;font-size:1rem;font-weight:600;
  border:0;border-radius:.4rem;background:#0a7;color:#fff}
button:disabled{opacity:.5}
#status{margin-top:.8rem;min-height:1.4rem}
.err{color:#c33}.ok{color:#0a7}
</style>
</head>
<body>
<h1>Variomètre — réglages</h1>
<form id="f"></form>
<button id="save" disabled>Enregistrer</button>
<div id="status">Chargement…</div>
<script>
const f=document.getElementById('f'),save=document.getElementById('save'),
  status=document.getElementById('status');
function input(fld){
  if(fld.type==='bool'){
    const i=document.createElement('input');i.type='checkbox';
    i.checked=fld.value>=.5;i.dataset.key=fld.key;return i;
  }
  if(fld.type==='enum'){
    const s=document.createElement('select');s.dataset.key=fld.key;
    [[0,fld.text&&fld.value<.5?fld.text:'0'],[1,'1']];
    for(const v of [0,1]){const o=document.createElement('option');
      o.value=v;o.textContent=v?'Moderne':'Classique';if(fld.value>=.5===!!v)
      o.selected=true;s.appendChild(o);}return s;
  }
  const i=document.createElement('input');i.type='number';
  i.min=fld.min;i.max=fld.max;i.step=fld.step;i.value=fld.value;
  i.dataset.key=fld.key;return i;
}
function render(schema){
  f.innerHTML='';
  for(const fld of schema.fields){
    const row=document.createElement('div');row.className='field';
    const lab=document.createElement('label');lab.textContent=fld.label;
    const unit=document.createElement('span');unit.className='unit';
    unit.textContent=fld.unit||'';
    row.append(lab,input(fld),unit);f.appendChild(row);
  }
  save.disabled=false;status.textContent='';
}
function collect(){
  const out={};
  for(const el of f.querySelectorAll('[data-key]')){
    out[el.dataset.key]=el.type==='checkbox'?(el.checked?1:0):Number(el.value);
  }
  return out;
}
fetch('/config').then(r=>r.json()).then(render)
  .catch(e=>{status.textContent='Lecture impossible';status.className='err';});
save.onclick=()=>{
  save.disabled=true;status.textContent='Envoi…';status.className='';
  fetch('/config',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(collect())})
    .then(r=>{if(!r.ok)throw 0;status.textContent='Enregistré';
      status.className='ok';})
    .catch(e=>{status.textContent='Échec de l\'enregistrement';
      status.className='err';})
    .finally(()=>{save.disabled=false;});
};
</script>
</body>
</html>)=====";

unsigned long configPageLength() {
  return static_cast<unsigned long>(std::strlen(kConfigPageHtml));
}

}  // namespace vario
