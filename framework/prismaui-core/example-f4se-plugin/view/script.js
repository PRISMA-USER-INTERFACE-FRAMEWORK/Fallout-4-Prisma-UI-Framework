function tab(n) {
    ['p','b','l','t'].forEach(function(x) {
        var el = document.getElementById('t'+x);
        if (el) el.classList.remove('active');
        var el2 = document.getElementById('tb'+x.toUpperCase());
        if (el2) el2.classList.remove('active');
    });
    var activeTab = document.getElementById('t'+n);
    if (activeTab) activeTab.classList.add('active');
    var activeBtn = document.getElementById('tb'+n.toUpperCase());
    if (activeBtn) activeBtn.classList.add('active');
}

var logEl = document.getElementById('log');
if (!logEl) {
    console.error('[Example Plugin] ERROR: log element not found in HTML — logging disabled');
    logEl = document.createElement('pre');
}
var logEmpty = true;
var logText = 'No events yet. Use the Papyrus Bridge or C++ Bridge tabs.';

window.sendDataToF4SE = window.sendDataToF4SE || function(data) {
    console.error('[Example Plugin] sendDataToF4SE not yet registered — please try again');
};

window.requestClose = window.requestClose || function() {
    console.warn('[Example Plugin] requestClose not yet registered — please try again');
};

function lg(cls, label, msg) {
    if (logEmpty) { logText = ''; logEmpty = false; }
    var t = new Date().toTimeString().slice(0,8);
    var entry = t + ' ' + label + ' ' + msg;
    if (logText.length > 0) logText += '\n';
    logText += entry;
    logEl.value = logText;
    logEl.scrollTop = logEl.scrollHeight;
}
function rv(id, s, t) {
    var e=document.getElementById(id);
    if (!e) { console.error('[Example Plugin] rv(): element '+id+' not found'); return; }
    e.className='rv '+s;
    e.textContent=t;
}

function doGetGlobal() {
    var gbEspEl=document.getElementById('gbEsp'), gbFidEl=document.getElementById('gbFid');
    if (!gbEspEl || !gbFidEl) { console.error('[Example Plugin] doGetGlobal: missing input elements'); return; }
    var esp=gbEspEl.value.trim(), fid=gbFidEl.value.trim();
    if (!esp||!fid) { rv('gbRes','error','enter esp and formId'); return; }
    if (!window.prisma) { rv('gbRes','error','window.prisma not available'); return; }
    rv('gbRes','waiting','…');
    lg('po','prisma→','getGlobal("'+esp+'","'+fid+'")');
    window.prisma.getGlobal(esp,fid).then(function(v) {
        if (v===null||v===undefined) { rv('gbRes','error','null — plugin absent or form not found'); lg('pi','←prisma','null'); }
        else { rv('gbRes','ok',String(v)); lg('pi','←prisma',''+v); }
    });
}
function doSetGlobal() {
    var gbEspEl=document.getElementById('gbEsp'), gbFidEl=document.getElementById('gbFid'), gbValEl=document.getElementById('gbVal');
    if (!gbEspEl || !gbFidEl || !gbValEl) { console.error('[Example Plugin] doSetGlobal: missing input elements'); return; }
    var esp=gbEspEl.value.trim(), fid=gbFidEl.value.trim();
    var raw=gbValEl.value.trim(), v=parseFloat(raw);
    if (!esp||!fid||raw===''||isNaN(v)) { lg('po','prisma→','setGlobal: missing/invalid fields'); return; }
    if (!window.prisma) { lg('po','prisma→','window.prisma not available'); return; }
    lg('po','prisma→','setGlobal("'+esp+'","'+fid+'",'+v+')');
    window.prisma.setGlobal(esp,fid,v);
}
function doGetProp() {
    var gpEspEl=document.getElementById('gpEsp'), gpFidEl=document.getElementById('gpFid'), gpScriptEl=document.getElementById('gpScript'), gpPropEl=document.getElementById('gpProp');
    if (!gpEspEl || !gpFidEl || !gpScriptEl || !gpPropEl) { console.error('[Example Plugin] doGetProp: missing input elements'); return; }
    var esp=gpEspEl.value.trim(), fid=gpFidEl.value.trim();
    var scr=gpScriptEl.value.trim(), prp=gpPropEl.value.trim();
    if (!esp||!fid||!scr||!prp) { rv('gpRes','error','fill in all four fields'); return; }
    if (!window.prisma) { rv('gpRes','error','window.prisma not available'); return; }
    rv('gpRes','waiting','…');
    lg('po','prisma→','getProperty("'+esp+'","'+fid+'","'+scr+'","'+prp+'")');
    window.prisma.getProperty(esp,fid,scr,prp).then(function(v) {
        if (v===null||v===undefined) { rv('gpRes','error','null — form/script/prop not found or VM not ready'); lg('pi','←prisma','null'); }
        else { rv('gpRes','ok',String(v)); lg('pi','←prisma',''+v); }
    });
}
function doSetProp() {
    var gpEspEl=document.getElementById('gpEsp'), gpFidEl=document.getElementById('gpFid'), gpScriptEl=document.getElementById('gpScript'), gpPropEl=document.getElementById('gpProp'), gpValEl=document.getElementById('gpVal');
    if (!gpEspEl || !gpFidEl || !gpScriptEl || !gpPropEl || !gpValEl) { console.error('[Example Plugin] doSetProp: missing input elements'); return; }
    var esp=gpEspEl.value.trim(), fid=gpFidEl.value.trim();
    var scr=gpScriptEl.value.trim(), prp=gpPropEl.value.trim();
    var raw=gpValEl.value.trim(), v=parseFloat(raw);
    if (!esp||!fid||!scr||!prp||raw===''||isNaN(v)) { lg('po','prisma→','setProperty: missing/invalid fields'); return; }
    if (!window.prisma) { lg('po','prisma→','window.prisma not available'); return; }
    lg('po','prisma→','setProperty("'+esp+'","'+fid+'","'+scr+'","'+prp+'",'+v+')');
    window.prisma.setProperty(esp,fid,scr,prp,v);
}


window.init = function() {
    var paBadge = document.getElementById('paBadge');
    var paText = document.getElementById('paText');
    var hdr = document.getElementById('hdr');
    if (!paBadge || !paText || !hdr) { console.error('[Example Plugin] init(): missing status elements'); return; }

    if (window.prisma) {
        paBadge.className='badge badge-green';
        paBadge.textContent='AVAILABLE';
        paText.textContent='window.prisma injected — ready to use';
        hdr.textContent='prisma ready';
    } else {
        paBadge.className='badge badge-red';
        paBadge.textContent='NOT FOUND';
        paText.textContent='window.prisma missing — check F4SE log';
        hdr.textContent='no prisma';
    }
    lg('cj','C++\u2192JS','init() \u2014 DOM ready');
};

window.updateFocusLabel = function(msg) {
    var focusLabel = document.getElementById('focusLabel');
    var focusBadge = document.getElementById('focusBadge');
    if (!focusLabel || !focusBadge) { console.error('[Example Plugin] updateFocusLabel(): missing focus elements'); return; }

    var focused = msg.indexOf('Focused') !== -1;
    focusLabel.textContent = msg;
    focusBadge.textContent = focused ? 'FOCUSED' : 'UNFOCUSED';
    focusBadge.className   = 'badge ' + (focused ? 'badge-green' : 'badge-amber');
    lg('cj','C++\u2192JS','updateFocusLabel("'+msg+'")');
};

window._resolveGlobalWrite = function(success) {
    var gbRes = document.getElementById('gbRes');
    if (!gbRes) return;
    if (success) {
        gbRes.className = 'rv ok';
        gbRes.textContent = 'Write successful';
        lg('pi','\u2190prisma','setGlobal success');
    } else {
        gbRes.className = 'rv error';
        gbRes.textContent = 'Write failed — form not found or not a global';
        lg('pi','\u2190prisma','setGlobal failed');
    }
};


window._prismaResolveGlobal = function(value) {
    var gbRes = document.getElementById('gbRes');
    if (!gbRes) return;
    if (value !== null && value !== undefined) {
        gbRes.className = 'rv ok';
        gbRes.textContent = String(value);
        lg('pi','\u2190prisma','getGlobal success: ' + value);
    } else {
        gbRes.className = 'rv error';
        gbRes.textContent = 'null — plugin absent or form not found';
        lg('pi','\u2190prisma','getGlobal failed');
    }
};

window._prismaResolveProperty = function(value) {
    var gpRes = document.getElementById('gpRes');
    if (!gpRes) return;
    if (value !== null && value !== undefined) {
        gpRes.className = 'rv ok';
        gpRes.textContent = String(value);
        lg('pi','\u2190prisma','getProperty success: ' + value);
    } else {
        gpRes.className = 'rv error';
        gpRes.textContent = 'null — form/script/prop not found or VM not ready';
        lg('pi','\u2190prisma','getProperty failed');
    }
};

function sendMsg() {
    var msgInEl = document.getElementById('msgIn');
    if (!msgInEl) { console.error('[Example Plugin] sendMsg(): msgIn element not found'); return; }
    var val = msgInEl.value.trim();
    if (!val) return;
    var p = JSON.stringify({message:val});
    lg('jc','JS\u2192C++','sendDataToF4SE('+p+')');
    window.sendDataToF4SE(p);
    msgInEl.value = '';
}
function closePanel() { lg('jc','JS\u2192C++','requestClose()'); window.requestClose(); }
function testConsole() {
    console.log('[PrismaUI Example] console.log test — check F4SE log');
    console.warn('[PrismaUI Example] console.warn test');
    lg('jc','JS\u2192C++','console.log + console.warn fired');
}
var _copyButtonState = { text: '', bg: '' };

function copyEventLog() {
    if (!logText) return;

    var copyBtn = document.querySelector('button[onclick="copyEventLog()"]');
    if (!copyBtn) return;

    _copyButtonState.text = copyBtn.textContent;
    _copyButtonState.bg = copyBtn.style.background;

    copyBtn.textContent = 'Copying…';
    copyBtn.style.background = 'rgba(34,197,94,0.7)';

    console.log('[copyEventLog] Sending to C++ bridge: ' + logText.length + ' bytes');
    lg('jc','JS\u2192C++','copyEventLog - sending ' + logText.length + ' bytes to C++');

    var payload = JSON.stringify({cmd: 'copyLog'}) + "|" + logText;
    window.sendDataToF4SE(payload);
}

window._resolveCopyLog = function(success) {
    var copyBtn = document.querySelector('button[onclick="copyEventLog()"]');

    if (success) {
        if (copyBtn) {
            copyBtn.textContent = 'Copied!';
            copyBtn.style.background = 'rgba(34,197,94,0.7)';
        }
        lg('pi','\u2190C++','copyLog success');
    } else {
        if (copyBtn) {
            copyBtn.textContent = 'Copy failed';
            copyBtn.style.background = 'rgba(239,68,68,0.7)';
        }
        lg('pi','\u2190C++','copyLog failed');
    }

    setTimeout(function() {
        if (copyBtn) {
            copyBtn.textContent = _copyButtonState.text;
            copyBtn.style.background = _copyButtonState.bg;
        }
    }, 1500);
};

window._showDiag = function(msg) {
    var gbRes = document.getElementById('gbRes');
    if (!gbRes) return;
    gbRes.className = 'rv ' + (msg.indexOf('ERROR') !== -1 ? 'error' : 'ok');
    gbRes.textContent = msg;
};

function doDiagnose() {
    var gbEspEl = document.getElementById('gbEsp');
    var gbFidEl = document.getElementById('gbFid');
    if (!gbEspEl || !gbFidEl) return;
    var esp = gbEspEl.value.trim();
    var fid = gbFidEl.value.trim();
    if (!esp || !fid) { alert('Enter Plugin and FormID first'); return; }
    lg('jc','JS\u2192C++','diagnose');
    window.sendDataToF4SE(JSON.stringify({cmd:'diagnose', esp: esp, formId: fid}));
}

var msgInEl = document.getElementById('msgIn');
if (msgInEl) {
    msgInEl.addEventListener('keydown', function(e) {
        if (e.key === 'Enter') sendMsg();
    });
}
