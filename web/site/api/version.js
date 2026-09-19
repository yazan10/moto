// Vercel Serverless - يتحكم بالأداة عن بعد - Kill Switch + Notifications
// يقرأ من ENV أو يرجع defaults قابلة للتعديل عبر Admin API
export default function handler(req, res) {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Cache-Control', 'no-store');

  // القيم الأساسية - الأدمن يغيرها عبر /api/admin/update
  const latest = process.env.LATEST_VERSION || '1.0.0';
  const minRequired = process.env.MIN_REQUIRED_VERSION || '1.0.0';
  const forceUpdate = (process.env.FORCE_UPDATE || 'false') === 'true';
  const downloadUrl = process.env.DOWNLOAD_URL || 'https://github.com/yazan10/moto/releases';
  const serverUrl = process.env.SERVER_URL || 'https://moto-site.vercel.app';

  // الإشعار الحالي (يُحدث من الأدمن)
  let notification = null;
  try {
    if (process.env.NOTIFICATION_JSON) {
      notification = JSON.parse(process.env.NOTIFICATION_JSON);
    }
  } catch(e){}

  // للسماح للأداة تفحص النسخة: ترجع JSON
  // الأداة تقارن APP_VERSION مع minRequired -> اذا أقل تتوقف
  res.status(200).json({
    latest,
    minRequired,
    forceUpdate,
    downloadUrl,
    serverUrl,
    notification: notification || { active: false, message: '' },
    timestamp: new Date().toISOString()
  });
}
