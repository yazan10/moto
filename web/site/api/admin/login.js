// POST /api/admin/login - تسجيل دخول الأدمن (مخفي)
// يقارن bcrypt hash المخزن في ENV

import bcrypt from 'bcryptjs';

export default async function handler(req,res){
  res.setHeader('Access-Control-Allow-Origin','*');
  if(req.method !== 'POST') return res.status(405).json({error:'POST only'});
  const { password } = req.body || {};
  if(!password) return res.status(400).json({error:'password required'});

  const hash = process.env.ADMIN_PASSWORD_HASH;
  if(!hash) return res.status(500).json({error:'ADMIN_PASSWORD_HASH not set in Vercel Env'});

  const ok = await bcrypt.compare(password, hash);
  if(!ok) return res.status(401).json({error:'wrong password'});

  // نرجع توكن مؤقت - في الإنتاج استخدم JWT أو ADMIN_TOKEN الثابت
  return res.status(200).json({ success:true, token: process.env.ADMIN_TOKEN });
}
