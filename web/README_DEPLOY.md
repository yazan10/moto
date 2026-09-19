# النشر الآمن - MotoLK by yaz

## 1. GitHub بدون كشف التوكن

```bash
cd /home/yaz/Downloads/Valhalla-Protocol-Motorola-Qt-main
git init
git add .
git commit -m "MotoLK Studio by yaz - site + admin + kill-switch"

# الطريقة الآمنة (لا تضع التوكن في الرابط)
gh auth login
git remote add origin https://github.com/yazan10/moto.git
git branch -M main
git push -u origin main
```

> لو استخدمت التوكن القديم ghp_... احذفه الآن من github.com/settings/tokens

## 2. Vercel - موقعين منفصلين بروابط مخصصة

### الموقع الرئيسي (site)
1. Vercel Dashboard -> Add New -> Project -> Import `yazan10/moto`
2. Root Directory = `web/site`
3. Framework Preset = Other
4. Env Variables:
   - `LATEST_VERSION=1.0.0`
   - `MIN_REQUIRED_VERSION=1.0.0`
   - `FORCE_UPDATE=false`
   - `DOWNLOAD_URL=https://moto-site.vercel.app/download`
   - `NOTIFICATION_JSON={"active":false,"message":""}`
   - `ADMIN_TOKEN=اختر_توكن_عشوائي_طويل_32حرف`
   - `ADMIN_PASSWORD_HASH=$2b$12$EukP1fhUmuFbtLPP/JZIfuLmWtPQ8I5N0sng92azxeWQDMgb3QtoK`  (هذا hash لـ yaz@#5)
5. Deploy -> سيعطيك رابط مثل `moto-site-xyz.vercel.app` -> اربطه بدومين مخصص من Settings -> Domains

### الأدمن المخفي (admin) - رابط منفصل
1. Vercel Dashboard -> Add New -> Project -> Import نفس الريبو `yazan10/moto` مرة ثانية
2. Root Directory = `web/admin`
3. نفس Env Variables (خصوصاً ADMIN_TOKEN و ADMIN_PASSWORD_HASH)
4. Deploy -> سيعطيك رابط ثاني منفصل مثل `moto-admin-abc.vercel.app`
5. **إخفاء إضافي:** Settings -> Deployment Protection -> Password Protection أو اجعل الرابط عشوائي مثل `moto-admin-9x7p2k.vercel.app` ولا تنشره

> روابط مخصصة: كل مشروع له رابط مختلف تماماً كما طلبت. الموقع العام + الأدمن كل واحد على Vercel Project مستقل.

## 3. كيف ترسل إشعار وتوقف النسخ القديمة

افتح رابط الأدمن المخفي -> سجل دخول `yaz@#5` -> غير:
- `latest` و `minRequired` 
- فعل `forceUpdate` = true
- اكتب رسالة الإشعار
-> اضغط "إرسال التحديث + الإشعار"

النتيجة:
- الموقع يظهر الشريط الأصفر بالإشعار
- الأداة عند التشغيل تستدعي `https://moto-site.vercel.app/api/version` -> اذا اصدارها < minRequired و forceUpdate=true -> تتوقف فوراً

## 4. ربط الأداة بالسيرفر
في `main.cpp` السطر: `#define SERVER_URL "https://moto-site.vercel.app/api/version"`
غيره لرابط موقعك الحقيقي بعد النشر.

السيرفر يظل مرتبط بالأداة حتى لو الأدمن غير الإصدار.

## 5. خط IBM Plex Arabic 700
مفعل في `web/site/index.html` و `web/admin/index.html`:
```html
<link href="https://fonts.googleapis.com/css2?family=IBM+Plex+Sans+Arabic:wght@700&display=swap" rel="stylesheet">
```
كل المواقع تستخدم `font-family:'IBM Plex Sans Arabic' weight 700`

## 6. حذف آثار GitHub
تم حذف كل روابط Extra-Team من README. لا يوجد ذكر لـ GitHub في الموقع العام.

## 7. حماية الأدمن
- الباسورد لا يوجد كنص في الكود، فقط hash bcrypt
- الدخول عبر `/api/admin/login` بفحص bcrypt
- التوكن `ADMIN_TOKEN` في Env فقط
- الرابط نفسه مخفي + `noindex` + يمكن تفعيل Vercel Protection

## ملاحظة أمان
غير التوكنات التي أرسلتها سابقاً فوراً قبل النشر.
