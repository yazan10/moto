// POST /api/admin/update - يحدث النسخة والإشعار ويُفعل Kill-Switch
// الحماية: يفحص x-admin-token == ADMIN_TOKEN (من Vercel Env)
// Body: { latest, minRequired, forceUpdate, notification: {active, message}, downloadUrl }

export default async function handler(req, res){
  res.setHeader('Access-Control-Allow-Origin','*');
  if(req.method === 'OPTIONS') return res.status(200).end();
  if(req.method !== 'POST') return res.status(405).json({error:'POST only'});

  const token = req.headers['x-admin-token'] || req.body?.token;
  if(!token || token !== process.env.ADMIN_TOKEN){
    return res.status(401).json({error:'Unauthorized - invalid admin token'});
  }

  // في Vercel الحقيقي نحدث ENV عبر Vercel API أو نستخدم Vercel KV/Upstash Redis
  // هنا نرجع تعليمات - التطبيق الفعلي يحتاج Vercel KV
  // للتبسيط: نستخدم Upstash Redis أو Vercel KV - المفاتيح تُحفظ هناك

  // مثال منطق الحفظ (يحتاج @vercel/kv):
  // await kv.set('app:version', { latest, minRequired, forceUpdate, downloadUrl, notification })

  // حالياً نرجع نجاح مع تذكير بإعداد KV
  const { latest, minRequired, forceUpdate, downloadUrl, notification } = req.body || {};

  // لو لم يكن KV مُعد، ننصح الأدمن يضيفه في Vercel Dashboard -> Storage -> KV
  return res.status(200).json({
    success: true,
    message: 'تم استلام التحديث - فعّل Vercel KV لحفظه بشكل دائم',
    received: { latest, minRequired, forceUpdate, downloadUrl, notification },
    hint: 'اربط Vercel KV من Dashboard ثم استبدل هذا الملف بمنطق kv.set/get',
    killSwitchActive: !!forceUpdate
  });
}
