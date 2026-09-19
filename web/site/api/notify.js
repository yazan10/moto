// GET /api/notify - ترجع الإشعار الحالي للأداة والموقع
export default function handler(req, res){
  res.setHeader('Access-Control-Allow-Origin','*');
  res.setHeader('Cache-Control','no-store');
  let note = { active: false, message: '' };
  try{ if(process.env.NOTIFICATION_JSON) note = JSON.parse(process.env.NOTIFICATION_JSON); }catch(e){}
  res.status(200).json(note);
}
