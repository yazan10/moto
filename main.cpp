#include <iostream>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QTimer>
#include <QUrl>

#include "types.h"
#include "utils.h"
#include "logger.h"
#include "keygen.h"
#include "lk_image.h"
#include "lk_repacker.h"
#include "mtk_cert.h"

// ===== Server-linked Kill-Switch & Notifications (by yaz) =====
#define APP_VERSION "1.0.0"
#define SERVER_URL "https://moto-site.vercel.app/api/version"

inline bool checkServerVersion(bool &shouldExit) {
    shouldExit = false;
    QNetworkAccessManager mgr;
    QNetworkRequest req(QUrl(SERVER_URL));
    req.setHeader(QNetworkRequest::UserAgentHeader, "MotoLK-Studio/" APP_VERSION);
    req.setRawHeader("X-App-Version", APP_VERSION);
    QNetworkReply *reply = mgr.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(4000); // 4s timeout - يبقى مرتبط بالسيرفر لكن لا يعلق الأداة
    loop.exec();
    if (!timer.isActive()) { // timeout
        reply->abort(); reply->deleteLater();
        std::cerr << "[Server] Warning: cannot reach update server, running in offline mode." << std::endl;
        return true;
    }
    timer.stop();
    if (reply->error() != QNetworkReply::NoError) {
        std::cerr << "[Server] Warning: " << reply->errorString().toStdString() << " - offline mode." << std::endl;
        reply->deleteLater(); return true;
    }
    QByteArray data = reply->readAll();
    reply->deleteLater();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return true;
    QJsonObject obj = doc.object();
    QString latest = obj.value("latest").toString(APP_VERSION);
    QString minRequired = obj.value("minRequired").toString(APP_VERSION);
    bool forceUpdate = obj.value("forceUpdate").toBool(false);
    QJsonObject notif = obj.value("notification").toObject();
    if (notif.value("active").toBool(false)) {
        std::cout << "\n📢 [Notification] " << notif.value("message").toString().toStdString() << "\n" << std::endl;
    }
    // مقارنة إصدارات بسيطة
    auto versionLess = [](const QString &a, const QString &b){
        QStringList pa = a.split('.'), pb = b.split('.');
        for(int i=0;i<qMax(pa.size(), pb.size());++i){
            int ia = i<pa.size()?pa[i].toInt():0;
            int ib = i<pb.size()?pb[i].toInt():0;
            if(ia<ib) return true; if(ia>ib) return false;
        } return false;
    };
    if (forceUpdate && versionLess(APP_VERSION, minRequired)) {
        std::cerr << "\n⛔ هذه النسخة منتهية. الإصدار المطلوب: " << minRequired.toStdString()
                  << " | آخر إصدار: " << latest.toStdString() << std::endl;
        std::cerr << "الرجاء التحميل من: " << obj.value("downloadUrl").toString().toStdString() << std::endl;
        std::cerr << "الأداة ستتوقف الآن (Kill-Switch مفعل من السيرفر)." << std::endl;
        shouldExit = true;
        return false;
    }
    if (versionLess(APP_VERSION, latest)) {
        std::cout << "[Update] يوجد إصدار جديد: " << latest.toStdString() << " (حالي: " APP_VERSION ")" << std::endl;
    }
    return true;
}

#ifdef VAL_HAS_ENGINE
#include "lk_pipeline.h"
#endif

using namespace ValCore;

int handleKeygen(const QStringList& args) {
    QCommandLineParser parser;
    parser.setApplicationDescription("Val Protocol Keygen Engine");

    QCommandLineOption secretOpt("secret", "Secret token used for derivation (default: Valeria)", "secret", "Valeria");
    QCommandLineOption serialOpt("serialno", "Device serial number from fastboot getvar serialno", "serialno");
    QCommandLineOption deviceOpt("device", "Device identifier", "device", "");
    QCommandLineOption imeiOpt("imei", "15-digit IMEI identifier", "imei", "");
    QCommandLineOption eraseOpt("erase-partition", "Generate tokens for protected erase of partition (e.g. frp, super)", "erase-partition", "");
    QCommandLineOption countOpt("count", "Number of keys to generate (default: 1)", "count", "1");
    QCommandLineOption alphaOpt("alphabet", "Alphabet: alnum or printable (default: alnum)", "alphabet", "alnum");
    QCommandLineOption safeOpt("safe", "Alias for --alphabet alnum");
    QCommandLineOption seedOpt("seed", "Deterministic seed for reproducible keys in testing", "seed", "");

    parser.addOption(secretOpt);
    parser.addOption(serialOpt);
    parser.addOption(deviceOpt);
    parser.addOption(imeiOpt);
    parser.addOption(eraseOpt);
    parser.addOption(countOpt);
    parser.addOption(alphaOpt);
    parser.addOption(safeOpt);
    parser.addOption(seedOpt);
    parser.process(args);

    QString secret = parser.value(secretOpt);
    QString serialno = parser.value(serialOpt);
    QString device = parser.value(deviceOpt);
    QString imei = parser.value(imeiOpt);
    QString erasePart = parser.value(eraseOpt);
    int count = parser.value(countOpt).toInt();
    if (count < 1) count = 1;

    KeygenAlphabet alpha = (parser.value(alphaOpt) == "printable" && !parser.isSet(safeOpt)) ? KeygenAlphabet::Printable : KeygenAlphabet::Alnum;
    std::optional<uint64_t> seedVal = std::nullopt;
    if (!parser.value(seedOpt).isEmpty()) {
        seedVal = parser.value(seedOpt).toULongLong();
    }

    try {
        QByteArray constants;
        QString modeStr;

        if (!erasePart.isEmpty()) {
            QString targetDevice = !imei.isEmpty() ? Keygen::normalizeImei(imei) : device;
            constants = Keygen::deriveEraseTokenConstants(secret, targetDevice, erasePart, alpha);
            modeStr = QString("erase-token (%1)").arg(erasePart);
        } else if (!serialno.isEmpty()) {
            constants = Keygen::deriveRuntimeSerialCompactConstants(secret, serialno);
            modeStr = "runtime-serial-compact";
        } else if (!imei.isEmpty()) {
            constants = Keygen::deriveImeiTokenConstants(secret, imei, alpha);
            modeStr = QString("imei-derived (%1)").arg(Keygen::normalizeImei(imei));
        } else {
            constants = Keygen::deriveTokenConstants(secret, device, alpha);
            modeStr = QString("device-derived (%1)").arg(device.isEmpty() ? "<global>" : device);
        }

        std::cout << "Mode     : " << modeStr.toStdString() << std::endl;
        std::cout << "Secret   : '" << secret.toStdString() << "'" << std::endl;
        if (!serialno.isEmpty()) std::cout << "Serialno : '" << serialno.toStdString() << "'" << std::endl;
        std::cout << "Constants: " << Utils::toHex(constants).toStdString() << std::endl;
        std::cout << "Keys (" << count << "):" << std::endl;

        QStringList keys = Keygen::generateUniqueKeys(constants, count, alpha, seedVal);
        for (const QString& k : keys) {
            std::cout << "  " << k.toStdString() << std::endl;
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Keygen Error: " << ex.what() << std::endl;
        return 1;
    }
}

int handleUnpack(const QString& imagePath, const QString& outDir) {
    LkImage img;
    QString error;
    if (!img.loadFromFile(imagePath, &error)) {
        std::cerr << "Error loading image: " << error.toStdString() << std::endl;
        return 1;
    }

    QDir dir(outDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    std::cout << "Unpacking " << img.partitions().size() << " partitions from " << imagePath.toStdString() << ":" << std::endl;
    for (const LkPartitionInfo& part : img.partitions()) {
        QString outFile = dir.filePath(QString("%1.bin").arg(part.name));
        QFile file(outFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(part.data);
            file.close();
            std::cout << "  -> Extracted: " << part.name.toStdString() 
                      << " (" << part.data.size() << " bytes) to " << outFile.toStdString() << std::endl;
        } else {
            std::cerr << "  -> Failed to write: " << outFile.toStdString() << std::endl;
        }
    }

    return 0;
}

int handleRepack(const QString& inImage, const QString& partName, const QString& newPartFile, const QString& outImage) {
    LkImage img;
    QString error;
    if (!img.loadFromFile(inImage, &error)) {
        std::cerr << "Error loading image: " << error.toStdString() << std::endl;
        return 1;
    }

    QFile file(newPartFile);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "Error reading partition payload: " << newPartFile.toStdString() << std::endl;
        return 1;
    }
    QByteArray newData = file.readAll();
    file.close();

    if (!img.replacePartitionData(partName, newData, &error)) {
        std::cerr << "Error replacing partition: " << error.toStdString() << std::endl;
        return 1;
    }

    if (!LkRepacker::repackToFile(img, outImage, &error)) {
        std::cerr << "Error repacking to file: " << error.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Successfully replaced '" << partName.toStdString() << "' (" << newData.size() << " bytes) and saved to " << outImage.toStdString() << std::endl;
    return 0;
}

int handleVerify(const QString& imagePath) {
    LkImage img;
    QString error;
    if (!img.loadFromFile(imagePath, &error)) {
        std::cerr << "Error: " << error.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Image size: " << img.totalSize() << " bytes" << std::endl;
    std::cout << "Partitions (" << img.partitions().size() << "):" << std::endl;
    for (const LkPartitionInfo& p : img.partitions()) {
        std::cout << "  - " << p.name.toStdString() 
                  << " (dsize: " << p.header.dsize << " bytes, pad: " << p.padding.size() 
                  << ", type: 0x" << QString::number(p.header.img_type, 16).toStdString() << ")" << std::endl;
    }

    CertVerificationResult res = MtkCert::verifyLkImageCert2(img, "lk");
    std::cout << "\nVerification:\n" << res.details.toStdString() << std::endl;
    return res.valid ? 0 : 1;
}

int handleSign(const QString& imagePath, const QString& outputPath) {
    LkImage img;
    QString error;
    if (!img.loadFromFile(imagePath, &error)) {
        std::cerr << "Error: " << error.toStdString() << std::endl;
        return 1;
    }

    if (!MtkCert::signLkImageCert2(img, "lk", &error)) {
        std::cerr << "Signing Error: " << error.toStdString() << std::endl;
        return 1;
    }

    if (!LkRepacker::repackToFile(img, outputPath, &error)) {
        std::cerr << "Repack Error: " << error.toStdString() << std::endl;
        return 1;
    }

    std::cout << "Successfully re-signed and saved to: " << outputPath.toStdString() << std::endl;
    return handleVerify(outputPath);
}

#ifdef VAL_HAS_ENGINE
int handlePatch(const QStringList& args) {
    if (args.contains("-h") || args.contains("--help")) {
        std::cout << "Usage: val_cli patch <in.img> -o <out.img> [options]\n"
                  << "Options:\n"
                  << "  --preset <preset>         Preset: unlock-serial, erase-serial, factory-allow, full-allow\n"
                  << "  --secret <secret>         Secret token (default: Valeria)\n"
                  << "  --erase-partition <name>  Target erase partition (default: frp)\n"
                  << "  --experimental            Allow unknown builds or layout drift overrides\n";
        return 0;
    }

    QCommandLineParser parser;
    parser.setApplicationDescription("Val Protocol Automated End-to-End Patcher");

    QCommandLineOption inputOpt({"i", "input"}, "Input original LK image", "image");
    QCommandLineOption outputOpt({"o", "output"}, "Output patched LK image", "output");
    QCommandLineOption presetOpt("preset", "Preset: unlock-serial, erase-serial, factory-allow, full-allow", "preset", "unlock-serial");
    QCommandLineOption secretOpt("secret", "Secret token (default: Valeria)", "secret", "Valeria");
    QCommandLineOption eraseOpt("erase-partition", "Erase partition target (default: frp)", "partition", "frp");
    QCommandLineOption expOpt("experimental", "Allow patching unknown builds or layout drift overrides");

    parser.addOption(inputOpt);
    parser.addOption(outputOpt);
    parser.addOption(presetOpt);
    parser.addOption(secretOpt);
    parser.addOption(eraseOpt);
    parser.addOption(expOpt);
    parser.process(args);

    QString inputPath = parser.value(inputOpt);
    QString outputPath = parser.value(outputOpt);

    // Also check positional args if options not given
    if (inputPath.isEmpty() && args.size() >= 3 && !args[2].startsWith("-")) {
        inputPath = args[2];
    }
    if (outputPath.isEmpty()) {
        for (int i = 2; i < args.size() - 1; ++i) {
            if (args[i] == "-o" || args[i] == "--output") {
                outputPath = args[i + 1];
                break;
            }
        }
    }

    if (inputPath.isEmpty() || outputPath.isEmpty()) {
        std::cerr << "Usage: val_cli patch <in.img> -o <out.img> [--preset <preset>] [--secret <secret>] [--experimental]" << std::endl;
        return 1;
    }

    PipelineConfig cfg;
    cfg.inputImagePath = inputPath;
    cfg.outputImagePath = outputPath;
    cfg.secret = parser.value(secretOpt);
    cfg.erasePartition = parser.value(eraseOpt);
    cfg.experimental = parser.isSet(expOpt);

    QString presetStr = parser.value(presetOpt);
    if (presetStr == "factory-allow") cfg.preset = PresetMode::FactoryAllow;
    else if (presetStr == "modem-unlock") cfg.preset = PresetMode::ModemUnlock;
    else if (presetStr == "full-allow") cfg.preset = PresetMode::FullAllow;
    else if (presetStr == "erase-serial" || presetStr == "unlock-serial-nvdata" || presetStr == "erase-imei") cfg.preset = PresetMode::EraseSerial;
    else cfg.preset = PresetMode::UnlockSerial;

    PipelineResult res = LkPipeline::run(cfg);
    return res.success ? 0 : 1;
}
#endif

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("val_cli");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    // فحص السيرفر - يبقى مرتبط بالسيرفر ويوقف النسخ القديمة
    bool shouldExit = false;
    checkServerVersion(shouldExit);
    if (shouldExit) return 1;

    QStringList args = app.arguments();
    if (args.size() < 2) {
        std::cout << "Val Protocol Native CLI (C++ Edition)\n"
                  << "Usage:\n"
                  << "  val_cli keygen --secret <secret> --serialno <serial> [--count N]\n"
                  << "  val_cli unpack <image.img> <out_dir>\n"
                  << "  val_cli repack <in.img> <part_name> <new_part.bin> <out.img>\n"
                  << "  val_cli verify <image.img>\n"
                  << "  val_cli sign <in.img> -o <out.img>\n"
#ifdef VAL_HAS_ENGINE
                  << "  val_cli patch <in.img> -o <out.img> [--preset <preset>] [--secret <secret>]\n"
#endif
                  ;
        return 0;
    }

    QString command = args[1];

    if (command == "-h" || command == "--help" || command == "help") {
        std::cout << "MotoLK Studio CLI v1.0.0 (by yaz)\n"
                  << "Usage:\n"
                  << "  motolk_cli keygen --secret <secret> --serialno <serial> [--count N]\n"
                  << "  motolk_cli unpack <image.img> <out_dir>\n"
                  << "  motolk_cli repack <in.img> <part_name> <new_part.bin> <out.img>\n"
                  << "  motolk_cli verify <image.img>\n"
                  << "  motolk_cli sign <in.img> -o <out.img>\n"
#ifdef VAL_HAS_ENGINE
                  << "  motolk_cli patch <in.img> -o <out.img> [--preset <preset>] [--secret <secret>]\n"
#endif
                  ;
        return 0;
    } else if (command == "keygen") {
        return handleKeygen(args);
    } else if (command == "unpack" && args.size() >= 4) {
        return handleUnpack(args[2], args[3]);
    } else if (command == "repack" && args.size() >= 6) {
        return handleRepack(args[2], args[3], args[4], args[5]);
    } else if (command == "verify" && args.size() >= 3) {
        return handleVerify(args[2]);
    } else if (command == "sign" && args.size() >= 5 && args[3] == "-o") {
        return handleSign(args[2], args[4]);
    }
#ifdef VAL_HAS_ENGINE
    else if (command == "patch") {
        return handlePatch(args);
    }
#endif

    std::cerr << "Unknown or incomplete command: " << command.toStdString() << std::endl;
    return 1;
}
