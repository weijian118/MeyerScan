#include "SettingsUIInternal.h"


// 创建信息管理页的单个标签页（医生/诊所/技工所共用布局）。
QWidget* SettingsUIImpl::CreateInfoTabPage(QWidget* parent,
                                           const QStringList& headers,
                                           const QList<QStringList>& rows) {
    auto* tab = new QWidget(parent);
    auto* layout = new QVBoxLayout(tab);
    // 标签页内部保留 8px 留白，防止表格线贴到 Tab 边界。
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 搜索栏 + 添加按钮行。
    auto* searchRow = new QHBoxLayout();
    auto* searchEdit = new QLineEdit(tab);
    // placeholder 使用 tr 包裹英文源文案，后续 lupdate 可提取翻译。
    searchEdit->setPlaceholderText(QWidget::tr("Search..."));
    searchEdit->setMinimumWidth(280);
    // 搜索框占左侧，右侧按钮固定宽度，Stretch 把两者分开。
    searchRow->addWidget(searchEdit);
    searchRow->addStretch();
    auto* addBtn = new QPushButton(QWidget::tr("Add"), tab);
    addBtn->setObjectName("SettingsPrimary");
    searchRow->addWidget(addBtn);
    layout->addLayout(searchRow);

    // 数据表格只展示 MainExe 注入的只读快照。
    // 新增、编辑、删除按钮目前只保留入口；保存逻辑后续统一走服务层。
    auto* table = new QTableWidget(tab);
    // QTableWidget 适合当前骨架期的少量只读快照；正式大数据量应切换到 model/view + 分页。
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        for (int c = 0; c < rows[r].size() && c < headers.size(); ++c) {
            // setItem 会接管 QTableWidgetItem 所有权，表格销毁时自动释放。
            // c < headers.size() 是防御字段扩展时行数据比表头多，避免越界写列。
            table->setItem(r, c, new QTableWidgetItem(rows[r][c]));
        }
    }
    // 选择整行比单格选择更适合后续“编辑/删除当前记录”的交互。
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 最后一列拉伸到剩余宽度，减少多分辨率下右侧空白。
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(200);
    layout->addWidget(table, 1);

    // 底部编辑/删除按钮。
    auto* actionRow = new QHBoxLayout();
    actionRow->addStretch();
    auto* editBtn = new QPushButton(QWidget::tr("Edit"), tab);
    auto* deleteBtn = new QPushButton(QWidget::tr("Delete"), tab);
    // 当前按钮只搭框架，不直接写数据库；后续应通过服务层弹出编辑对话框或执行删除。
    actionRow->addWidget(editBtn);
    actionRow->addWidget(deleteBtn);
    layout->addLayout(actionRow);

    return tab;
}

// 创建"信息管理"设置页面。
// 使用 QTabWidget 展示医生/诊所/技工所三个标签页，每个标签页包含搜索栏、
// 数据表格和编辑/删除按钮。数据来源是 MainExe 注入的只读快照。
QWidget* SettingsUIImpl::CreateInfoPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);
    // 外层不留边距，让内部 QTabWidget 与设置内容区自然对齐。
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* tabs = new QTabWidget(page);
    // documentMode 让 Tab 更像设置页的内部标签，而不是主页面级导航。
    tabs->setDocumentMode(true);

    // 医生管理标签页。
    {
        QStringList headers;
        // 表头是 UI 层稳定文案；真实字段名由 BuildDoctorRows 内部兼容旧库字段。
        headers << tr("Name") << tr("Gender") << tr("Phone") << tr("Department");
        const QList<QStringList> rows = BuildDoctorRows(LoadContextItems("local.doctors"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Doctors"));
    }

    // 诊所管理标签页。
    {
        QStringList headers;
        // 诊所页只展示常用字段，完整字段后续可在编辑弹窗或详情页按需展示。
        headers << tr("Name") << tr("Address") << tr("Phone") << tr("City");
        const QList<QStringList> rows = BuildClinicRows(LoadContextItems("local.clinics"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Clinics"));
    }

    // 技工所管理标签页。
    {
        QStringList headers;
        // 技工所页展示合作方基础信息，云端 ID 等字段暂不直接放在列表里。
        headers << tr("Name") << tr("Contact") << tr("Phone") << tr("Address");
        const QList<QStringList> rows = BuildLabRows(LoadContextItems("local.labs"));
        tabs->addTab(CreateInfoTabPage(tabs, headers, rows), tr("Dental Labs"));
    }

    layout->addWidget(tabs, 1);
    return page;
}

// 从宿主注入的数据上下文读取指定 domain 的 items 数组。
QJsonArray SettingsUIImpl::LoadContextItems(const char* domain) const {
    // SettingsUI 只认稳定 domain，不认数据库表名、SQL 或 RuntimeDataCenter 接口。
    if (!domain || !domain[0]) {
        return QJsonArray();
    }
    const QJsonObject domains = m_dataContext.value("domains").toObject();
    return domains.value(QString::fromUtf8(domain)).toObject().value("items").toArray();
}

// 把医生快照转换成信息管理表格行。
QList<QStringList> SettingsUIImpl::BuildDoctorRows(const QJsonArray& items) const {
    QList<QStringList> rows;
    for (const QJsonValue& value : items) {
        // 每个 value 是宿主快照中的单行 JSON 对象；字段可能来自旧库映射或未来新数据源。
        const QJsonObject item = value.toObject();
        if (item.isEmpty()) {
            // 非对象或空对象不展示，避免把坏数据渲染成一行空白。
            continue;
        }

        const QString genderValue = FirstText(item, QStringList() << "DENTIST_SEX" << "PATIENT_SEX" << "gender");
        QString genderText = genderValue;
        // 旧库中性别常用数字编码；这里在 UI 层转成可翻译文本。
        if (genderValue == "1") {
            genderText = tr("Male");
        } else if (genderValue == "2") {
            genderText = tr("Female");
        }

        rows << (QStringList()
                 << FirstText(item, QStringList() << "DENTIST_NAME" << "name")
                 << genderText
                 << FirstText(item, QStringList() << "DENTIST_TEL" << "phone")
                 << FirstText(item, QStringList() << "DENTIST_PRO" << "department"));
    }
    return rows;
}

// 把诊所快照转换成信息管理表格行。
QList<QStringList> SettingsUIImpl::BuildClinicRows(const QJsonArray& items) const {
    QList<QStringList> rows;
    for (const QJsonValue& value : items) {
        // FirstText 支持多个字段名，便于旧表字段和未来新字段共存。
        const QJsonObject item = value.toObject();
        if (item.isEmpty()) {
            // 旧库数据异常时跳过这一行，表格仍然能显示其它正常记录。
            continue;
        }

        rows << (QStringList()
                 << FirstText(item, QStringList() << "CLINIC_NAME" << "name")
                 << FirstText(item, QStringList() << "CLINIC_DETAILADDRESS" << "CLINIC_ADDRESS" << "CLINIC_LOCATION" << "address")
                 << FirstText(item, QStringList() << "CLINIC_TEL" << "phone")
                 << FirstText(item, QStringList() << "CLINIC_CITY" << "city"));
    }
    return rows;
}

// 把技工所快照转换成信息管理表格行。
QList<QStringList> SettingsUIImpl::BuildLabRows(const QJsonArray& items) const {
    QList<QStringList> rows;
    for (const QJsonValue& value : items) {
        // 技工所字段在不同版本旧库中可能不一致，候选 key 放在 FirstText 中集中处理。
        const QJsonObject item = value.toObject();
        if (item.isEmpty()) {
            // 跳过空对象，避免表格出现用户无法理解的空白记录。
            continue;
        }

        rows << (QStringList()
                 << FirstText(item, QStringList() << "LAB_NAME" << "name")
                 << FirstText(item, QStringList() << "LAB_CONTACT" << "contact")
                 << FirstText(item, QStringList() << "LAB_TEL" << "phone")
                 << FirstText(item, QStringList() << "LAB_ADDRESS" << "address"));
    }
    return rows;
}

// 从 JSON 对象读取第一个非空字段。
QString SettingsUIImpl::FirstText(const QJsonObject& object, const QStringList& keys) const {
    // 这是信息页的字段兼容工具：按候选 key 顺序取第一个可显示值。
    // 这样新增字段或旧字段改名时，只改这里调用处的 key 列表，不改表格填充流程。
    for (const QString& key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isString()) {
            // 空字符串不适合显示，trim 后仍为空就继续尝试下一个候选字段。
            const QString text = value.toString().trimmed();
            if (!text.isEmpty()) {
                return text;
            }
        } else if (value.isDouble()) {
            // Qt JSON 用 double 表示所有数字，旧库 ID/编码按整数文本展示。
            return QString::number(value.toDouble(), 'f', 0);
        } else if (value.isBool()) {
            // bool 转 true/false，便于后续配置类字段直观看到状态。
            return value.toBool() ? "true" : "false";
        }
    }
    return QString();
}
