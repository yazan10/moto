// يفحص السيرفر للنسخة والإشعارات - مرتبط بالأداة
const API_BASE = window.location.origin; // نفس دومين Vercel
async function checkVersion(){
  try{
    const r = await fetch(API_BASE + '/api/version');
    const data = await r.json();
    document.getElementById('version-info').textContent = `آخر إصدار: ${data.latest} | إصدارك: ${data.latest} ${data.forceUpdate ? '(تحديث إجباري)' : ''}`;
    const dl = document.getElementById('download-link');
    const dl2 = document.getElementById('download-btn');
    if(data.downloadUrl){ dl.href = data.downloadUrl; dl2.href = data.downloadUrl; }
    // إشعار
    if(data.notification && data.notification.active){
      const bar = document.getElementById('notify-bar');
      bar.textContent = '📢 ' + data.notification.message;
      bar.classList.remove('hidden');
    }
    // Kill-switch للويب (تنبيه)
    if(data.forceUpdate){
      console.warn('Force update enabled - old tool versions will stop');
    }
  }catch(e){
    document.getElementById('version-info').textContent = 'تعذر الاتصال بالسيرفر - سيتم المحاولة لاحقاً';
  }
}
checkVersion();
