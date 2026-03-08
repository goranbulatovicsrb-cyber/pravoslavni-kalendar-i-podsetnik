#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <ctime>
#include <iomanip>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

using std::string;
using std::vector;
using std::wstring;

#define APP_TITLE L"Orthodox Reminder Pro v3"
#define APP_CLASS L"OrthodoxReminderProV3Class"

#define IDC_MONTHCAL 101
#define IDC_HOLIDAYLIST 102
#define IDC_REMINDERLIST 103
#define IDC_TITLEEDIT 104
#define IDC_NOTEEDIT 105
#define IDC_RECURCOMBO 106
#define IDC_ADDREM 107
#define IDC_UPDATEREM 108
#define IDC_DELETEREM 109
#define IDC_SEARCHEDIT 110
#define IDC_SEARCHBTN 111
#define IDC_UPCOMINGBTN 112
#define IDC_YEARBTN 113
#define IDC_EXPORTDAYBTN 114
#define IDC_EXPORTYEARBTN 115
#define IDC_BACKUPBTN 116
#define IDC_RESTOREBTN 117
#define IDC_CLEARBTN 118
#define IDC_STATUSBAR 119
#define IDC_YEARLIST 120
#define IDC_NOTIFYSTART 121

struct Date {
    int y = 0, m = 0, d = 0;
};

enum class Recurrence {
    Once,
    Daily,
    Weekly,
    Monthly,
    Yearly
};

struct Reminder {
    int id = 0;
    Date date;
    string title;
    string note;
    Recurrence recurrence = Recurrence::Once;
    bool done = false;
};

static HINSTANCE g_hInst = nullptr;
static HWND g_hMain = nullptr;
static HWND g_hCal = nullptr;
static HWND g_hHolidayList = nullptr;
static HWND g_hReminderList = nullptr;
static HWND g_hTitleEdit = nullptr;
static HWND g_hNoteEdit = nullptr;
static HWND g_hRecurrence = nullptr;
static HWND g_hSearchEdit = nullptr;
static HWND g_hStatus = nullptr;
static vector<Reminder> g_reminders;
static int g_nextId = 1;
static int g_selectedReminderId = -1;
static bool g_notifyOnStart = true;

static wstring s2ws(const string& s) {
    if (s.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    wstring ws(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws.data(), size);
    return ws;
}

static string ws2s(const wstring& ws) {
    if (ws.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    string s(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, s.data(), size, nullptr, nullptr);
    return s;
}

static wstring GetWindowTextString(HWND hWnd) {
    int len = GetWindowTextLengthW(hWnd);
    wstring text(len, L'\0');
    GetWindowTextW(hWnd, text.data(), len + 1);
    return text;
}

static string Trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool operator==(const Date& a, const Date& b) { return a.y == b.y && a.m == b.m && a.d == b.d; }
static bool operator<(const Date& a, const Date& b) {
    if (a.y != b.y) return a.y < b.y;
    if (a.m != b.m) return a.m < b.m;
    return a.d < b.d;
}

static bool IsLeap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int DaysInMonth(int y, int m) {
    static int md[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2) return IsLeap(y) ? 29 : 28;
    return md[m - 1];
}

static Date Today() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return { (int)st.wYear, (int)st.wMonth, (int)st.wDay };
}

static Date SelectedDate() {
    SYSTEMTIME st{};
    MonthCal_GetCurSel(g_hCal, &st);
    return { (int)st.wYear, (int)st.wMonth, (int)st.wDay };
}

static int DateToOrdinal(const Date& dt) {
    std::tm tm{};
    tm.tm_year = dt.y - 1900;
    tm.tm_mon = dt.m - 1;
    tm.tm_mday = dt.d;
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    return (int)(t / 86400);
}

static int DaysBetween(const Date& a, const Date& b) {
    return DateToOrdinal(b) - DateToOrdinal(a);
}

static Date AddDays(const Date& dt, int days) {
    std::tm tm{};
    tm.tm_year = dt.y - 1900;
    tm.tm_mon = dt.m - 1;
    tm.tm_mday = dt.d + days;
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    std::tm* out = localtime(&t);
    return { out->tm_year + 1900, out->tm_mon + 1, out->tm_mday };
}

static string DateToISO(const Date& dt) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << dt.y << '-' << std::setw(2) << dt.m << '-' << std::setw(2) << dt.d;
    return oss.str();
}

static string RecurrenceToString(Recurrence r) {
    switch (r) {
        case Recurrence::Once: return "Jednom";
        case Recurrence::Daily: return "Dnevno";
        case Recurrence::Weekly: return "Nedeljno";
        case Recurrence::Monthly: return "Mesecno";
        case Recurrence::Yearly: return "Godisnje";
    }
    return "Jednom";
}

static Recurrence IndexToRecurrence(int idx) {
    switch (idx) {
        case 1: return Recurrence::Daily;
        case 2: return Recurrence::Weekly;
        case 3: return Recurrence::Monthly;
        case 4: return Recurrence::Yearly;
        default: return Recurrence::Once;
    }
}

static int RecurrenceToIndex(Recurrence r) {
    switch (r) {
        case Recurrence::Daily: return 1;
        case Recurrence::Weekly: return 2;
        case Recurrence::Monthly: return 3;
        case Recurrence::Yearly: return 4;
        default: return 0;
    }
}

static bool ReminderOccursOn(const Reminder& r, const Date& date) {
    if (r.recurrence == Recurrence::Once) return r.date == date;
    if (date < r.date) return false;
    int delta = DaysBetween(r.date, date);
    switch (r.recurrence) {
        case Recurrence::Daily: return delta >= 0;
        case Recurrence::Weekly: return delta >= 0 && delta % 7 == 0;
        case Recurrence::Monthly: return date.d == r.date.d || (r.date.d > DaysInMonth(date.y, date.m) && date.d == DaysInMonth(date.y, date.m));
        case Recurrence::Yearly: return date.m == r.date.m && date.d == r.date.d;
        default: return r.date == date;
    }
}

static string Escape(const string& s) {
    string out;
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == '|') out += "\\p";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

static string Unescape(const string& s) {
    string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            if (n == '\\') out += '\\';
            else if (n == 'p') out += '|';
            else if (n == 'n') out += '\n';
            else out += n;
        } else out += s[i];
    }
    return out;
}

static string DataFilePath() {
    wchar_t path[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path);
    wstring dir = wstring(path) + L"\\OrthodoxReminderPro";
    CreateDirectoryW(dir.c_str(), nullptr);
    return ws2s(dir + L"\\reminders_v3.txt");
}

static string BackupDir() {
    wchar_t path[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, path);
    return ws2s(wstring(path));
}

static void SaveData() {
    std::ofstream out(DataFilePath(), std::ios::binary);
    out << "NEXTID|" << g_nextId << "\n";
    out << "SETTINGS|notify|" << (g_notifyOnStart ? 1 : 0) << "\n";
    for (const auto& r : g_reminders) {
        out << "REM|" << r.id << '|'
            << r.date.y << '|' << r.date.m << '|' << r.date.d << '|'
            << (int)r.recurrence << '|' << (r.done ? 1 : 0) << '|'
            << Escape(r.title) << '|' << Escape(r.note) << "\n";
    }
}

static void LoadData() {
    g_reminders.clear();
    g_nextId = 1;
    g_notifyOnStart = true;
    std::ifstream in(DataFilePath(), std::ios::binary);
    if (!in) return;
    string line;
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        vector<string> parts;
        string part;
        while (std::getline(ss, part, '|')) parts.push_back(part);
        if (parts.empty()) continue;
        if (parts[0] == "NEXTID" && parts.size() >= 2) {
            g_nextId = std::max(1, atoi(parts[1].c_str()));
        } else if (parts[0] == "SETTINGS" && parts.size() >= 3 && parts[1] == "notify") {
            g_notifyOnStart = atoi(parts[2].c_str()) != 0;
        } else if (parts[0] == "REM" && parts.size() >= 9) {
            Reminder r;
            r.id = atoi(parts[1].c_str());
            r.date.y = atoi(parts[2].c_str());
            r.date.m = atoi(parts[3].c_str());
            r.date.d = atoi(parts[4].c_str());
            r.recurrence = (Recurrence)atoi(parts[5].c_str());
            r.done = atoi(parts[6].c_str()) != 0;
            r.title = Unescape(parts[7]);
            r.note = Unescape(parts[8]);
            g_reminders.push_back(r);
            g_nextId = std::max(g_nextId, r.id + 1);
        }
    }
}

static Date OrthodoxEaster(int year) {
    int a = year % 4;
    int b = year % 7;
    int c = year % 19;
    int d = (19 * c + 15) % 30;
    int e = (2 * a + 4 * b - d + 34) % 7;
    int month = (d + e + 114) / 31;
    int day = ((d + e + 114) % 31) + 1;
    Date julian{year, month, day};
    return AddDays(julian, 13);
}

static vector<std::pair<Date, string>> BuildHolidayList(int year) {
    vector<std::pair<Date, string>> h;
    auto add = [&](int m, int d, const string& name) { h.push_back({{year,m,d}, name}); };
    add(1, 7, "Bozic");
    add(1, 14, "Mali Bozic / Sv. Vasilije");
    add(1, 19, "Bogojavljenje");
    add(1, 27, "Sveti Sava");
    add(2, 15, "Sretenje Gospodnje");
    add(4, 7, "Blagovesti");
    add(5, 6, "Djurdjevdan");
    add(7, 7, "Ivanjdan");
    add(7, 12, "Petrovdan");
    add(8, 19, "Preobrazenje");
    add(8, 28, "Velika Gospojina");
    add(9, 21, "Mala Gospojina");
    add(9, 27, "Krstovdan / Vozdvizenje Casnog Krsta");
    add(10, 14, "Pokrov Presvete Bogorodice");
    add(11, 8, "Mitrovdan");
    add(11, 21, "Arandjelovdan");
    add(12, 19, "Sveti Nikola");

    Date easter = OrthodoxEaster(year);
    h.push_back({AddDays(easter, -48), "Pocetak Vaskrsnjeg posta"});
    h.push_back({AddDays(easter, -7), "Cveti"});
    h.push_back({AddDays(easter, -2), "Veliki petak"});
    h.push_back({easter, "Vaskrs"});
    h.push_back({AddDays(easter, 1), "Vaskrsnji ponedeljak"});
    h.push_back({AddDays(easter, 39), "Spasovdan"});
    h.push_back({AddDays(easter, 49), "Duhovi"});

    std::sort(h.begin(), h.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return h;
}

static vector<string> HolidaysOnDate(const Date& dt) {
    vector<string> out;
    auto h = BuildHolidayList(dt.y);
    for (const auto& item : h) if (item.first == dt) out.push_back(item.second);
    return out;
}

static void SetStatus(const wstring& text) {
    SendMessageW(g_hStatus, SB_SETTEXT, 0, (LPARAM)text.c_str());
}

static vector<const Reminder*> RemindersForDate(const Date& dt) {
    vector<const Reminder*> out;
    for (const auto& r : g_reminders) if (ReminderOccursOn(r, dt)) out.push_back(&r);
    std::sort(out.begin(), out.end(), [](const Reminder* a, const Reminder* b) {
        if (a->done != b->done) return !a->done;
        if (a->title != b->title) return a->title < b->title;
        return a->id < b->id;
    });
    return out;
}

static void FillReminderList(const Date& dt) {
    ListView_DeleteAllItems(g_hReminderList);
    auto reminders = RemindersForDate(dt);
    int row = 0;
    for (const auto* r : reminders) {
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = row;
        item.lParam = r->id;
        wstring title = s2ws(r->title);
        item.pszText = title.data();
        ListView_InsertItem(g_hReminderList, &item);
        ListView_SetItemText(g_hReminderList, row, 1, (LPWSTR)s2ws(RecurrenceToString(r->recurrence)).c_str());
        ListView_SetItemText(g_hReminderList, row, 2, (LPWSTR)s2ws(r->done ? "Zavrseno" : "Aktivno").c_str());
        ListView_SetItemText(g_hReminderList, row, 3, (LPWSTR)s2ws(r->note).c_str());
        ++row;
    }
}

static void FillHolidayList(const Date& dt) {
    ListView_DeleteAllItems(g_hHolidayList);
    auto holidays = HolidaysOnDate(dt);
    if (holidays.empty()) holidays.push_back("Nema unetog velikog praznika za ovaj datum.");
    int row = 0;
    for (const auto& h : holidays) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row++;
        wstring ws = s2ws(h);
        item.pszText = ws.data();
        ListView_InsertItem(g_hHolidayList, &item);
    }
}

static void ClearEditors() {
    SetWindowTextW(g_hTitleEdit, L"");
    SetWindowTextW(g_hNoteEdit, L"");
    SendMessageW(g_hRecurrence, CB_SETCURSEL, 0, 0);
    g_selectedReminderId = -1;
}

static Reminder* FindReminderById(int id) {
    for (auto& r : g_reminders) if (r.id == id) return &r;
    return nullptr;
}

static void RefreshAll() {
    Date dt = SelectedDate();
    FillHolidayList(dt);
    FillReminderList(dt);
    std::wstringstream ss;
    ss << L"Datum: " << dt.d << L'.' << dt.m << L'.' << dt.y << L" | Podsetnici: " << RemindersForDate(dt).size();
    SetStatus(ss.str());
}

static void AddColumns(HWND hList, const vector<wstring>& headers, const vector<int>& widths) {
    for (size_t i = 0; i < headers.size(); ++i) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = const_cast<LPWSTR>(headers[i].c_str());
        col.cx = widths[i];
        col.iSubItem = (int)i;
        ListView_InsertColumn(hList, (int)i, &col);
    }
}

static void SaveAndRefresh(const wstring& statusText) {
    SaveData();
    RefreshAll();
    SetStatus(statusText);
}

static void AddReminderFromUI() {
    string title = Trim(ws2s(GetWindowTextString(g_hTitleEdit)));
    string note = Trim(ws2s(GetWindowTextString(g_hNoteEdit)));
    if (title.empty()) {
        MessageBoxW(g_hMain, L"Upisi naslov podsetnika.", APP_TITLE, MB_OK | MB_ICONWARNING);
        return;
    }
    Reminder r;
    r.id = g_nextId++;
    r.date = SelectedDate();
    r.title = title;
    r.note = note;
    r.recurrence = IndexToRecurrence((int)SendMessageW(g_hRecurrence, CB_GETCURSEL, 0, 0));
    g_reminders.push_back(r);
    ClearEditors();
    SaveAndRefresh(L"Podsetnik dodat.");
}

static void UpdateReminderFromUI() {
    if (g_selectedReminderId < 0) {
        MessageBoxW(g_hMain, L"Izaberi podsetnik iz liste.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return;
    }
    Reminder* r = FindReminderById(g_selectedReminderId);
    if (!r) return;
    string title = Trim(ws2s(GetWindowTextString(g_hTitleEdit)));
    if (title.empty()) {
        MessageBoxW(g_hMain, L"Naslov ne sme biti prazan.", APP_TITLE, MB_OK | MB_ICONWARNING);
        return;
    }
    r->title = title;
    r->note = Trim(ws2s(GetWindowTextString(g_hNoteEdit)));
    r->recurrence = IndexToRecurrence((int)SendMessageW(g_hRecurrence, CB_GETCURSEL, 0, 0));
    SaveAndRefresh(L"Podsetnik izmenjen.");
}

static void DeleteSelectedReminder() {
    if (g_selectedReminderId < 0) return;
    int res = MessageBoxW(g_hMain, L"Obrisati izabrani podsetnik?", APP_TITLE, MB_YESNO | MB_ICONQUESTION);
    if (res != IDYES) return;
    g_reminders.erase(std::remove_if(g_reminders.begin(), g_reminders.end(), [](const Reminder& r) { return r.id == g_selectedReminderId; }), g_reminders.end());
    ClearEditors();
    SaveAndRefresh(L"Podsetnik obrisan.");
}

static void ToggleDoneSelected() {
    if (g_selectedReminderId < 0) return;
    Reminder* r = FindReminderById(g_selectedReminderId);
    if (!r) return;
    r->done = !r->done;
    SaveAndRefresh(r->done ? L"Podsetnik oznacen kao zavrsen." : L"Podsetnik vracen u aktivne.");
}

static void LoadReminderIntoEditors(int id) {
    Reminder* r = FindReminderById(id);
    if (!r) return;
    g_selectedReminderId = id;
    SetWindowTextW(g_hTitleEdit, s2ws(r->title).c_str());
    SetWindowTextW(g_hNoteEdit, s2ws(r->note).c_str());
    SendMessageW(g_hRecurrence, CB_SETCURSEL, RecurrenceToIndex(r->recurrence), 0);
}

static void ExportDailyReport() {
    Date dt = SelectedDate();
    string path = BackupDir() + "\\daily_report_" + DateToISO(dt) + ".txt";
    std::ofstream out(path);
    out << "Orthodox Reminder Pro v3\n";
    out << "Datum: " << DateToISO(dt) << "\n\n";
    out << "PRAZNICI\n";
    for (auto& h : HolidaysOnDate(dt)) out << "- " << h << "\n";
    out << "\nPODSETNICI\n";
    for (auto* r : RemindersForDate(dt)) {
        out << "- [" << (r->done ? 'x' : ' ') << "] " << r->title << " (" << RecurrenceToString(r->recurrence) << ")\n";
        if (!r->note.empty()) out << "  " << r->note << "\n";
    }
    MessageBoxW(g_hMain, s2ws("Dnevni izvestaj sacuvan na Desktop: " + path).c_str(), APP_TITLE, MB_OK | MB_ICONINFORMATION);
}

static void ExportYearReport() {
    Date dt = SelectedDate();
    auto holidays = BuildHolidayList(dt.y);
    string path = BackupDir() + "\\year_report_" + std::to_string(dt.y) + ".txt";
    std::ofstream out(path);
    out << "Godisnji pravoslavni kalendar - " << dt.y << "\n\n";
    for (const auto& h : holidays) out << DateToISO(h.first) << " - " << h.second << "\n";
    MessageBoxW(g_hMain, s2ws("Godisnji izvestaj sacuvan na Desktop: " + path).c_str(), APP_TITLE, MB_OK | MB_ICONINFORMATION);
}

static void ExportCSVUpcoming() {
    Date start = Today();
    Date end = AddDays(start, 120);
    string path = BackupDir() + "\\upcoming_120_days.csv";
    std::ofstream out(path);
    out << "datum,naslov,ponavljanje,status,napomena\n";
    for (Date cur = start; !(end < cur); cur = AddDays(cur, 1)) {
        for (auto* r : RemindersForDate(cur)) {
            out << DateToISO(cur) << ',' << '"' << r->title << '"' << ',' << '"' << RecurrenceToString(r->recurrence) << '"' << ',' << '"' << (r->done ? "zavrseno" : "aktivno") << '"' << ',' << '"' << r->note << '"' << "\n";
        }
    }
    MessageBoxW(g_hMain, s2ws("CSV predstojećih obaveza sacuvan na Desktop: " + path).c_str(), APP_TITLE, MB_OK | MB_ICONINFORMATION);
}

static void BackupData() {
    string src = DataFilePath();
    string dst = BackupDir() + "\\orthodox_reminder_backup.txt";
    if (CopyFileW(s2ws(src).c_str(), s2ws(dst).c_str(), FALSE)) {
        MessageBoxW(g_hMain, s2ws("Backup sacuvan na Desktop: " + dst).c_str(), APP_TITLE, MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(g_hMain, L"Backup nije uspeo.", APP_TITLE, MB_OK | MB_ICONERROR);
    }
}

static void RestoreData() {
    string src = BackupDir() + "\\orthodox_reminder_backup.txt";
    string dst = DataFilePath();
    if (CopyFileW(s2ws(src).c_str(), s2ws(dst).c_str(), FALSE)) {
        LoadData();
        RefreshAll();
        MessageBoxW(g_hMain, L"Podaci vraceni iz backup fajla sa Desktop-a.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(g_hMain, L"Backup fajl nije pronadjen na Desktop-u.", APP_TITLE, MB_OK | MB_ICONWARNING);
    }
}

static void ShowYearWindow(HWND parent) {
    Date dt = SelectedDate();
    auto holidays = BuildHolidayList(dt.y);
    string text = "Godisnji pregled praznika za " + std::to_string(dt.y) + "\n\n";
    for (auto& h : holidays) text += DateToISO(h.first) + " - " + h.second + "\n";
    MessageBoxW(parent, s2ws(text).c_str(), L"Godisnji pregled", MB_OK | MB_ICONINFORMATION);
}

static void ShowUpcomingWindow(HWND parent) {
    Date start = Today();
    Date end = AddDays(start, 90);
    string text = "Predstojece obaveze za narednih 90 dana\n\n";
    int count = 0;
    for (Date cur = start; !(end < cur); cur = AddDays(cur, 1)) {
        auto rems = RemindersForDate(cur);
        for (auto* r : rems) {
            text += DateToISO(cur) + " - " + r->title + " [" + RecurrenceToString(r->recurrence) + "]" + (r->done ? " [zavrseno]" : "") + "\n";
            ++count;
        }
    }
    if (count == 0) text += "Nema zabelezenih obaveza.\n";
    MessageBoxW(parent, s2ws(text).c_str(), L"Predstojece obaveze", MB_OK | MB_ICONINFORMATION);
}

static void SearchReminders() {
    string query = Trim(ws2s(GetWindowTextString(g_hSearchEdit)));
    if (query.empty()) {
        MessageBoxW(g_hMain, L"Upisi termin za pretragu.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    string text = "Rezultati pretrage\n\n";
    int found = 0;
    for (const auto& r : g_reminders) {
        string hay = r.title + "\n" + r.note;
        string low = hay;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        if (low.find(query) != string::npos) {
            text += DateToISO(r.date) + " - " + r.title + " [" + RecurrenceToString(r.recurrence) + "]\n";
            if (!r.note.empty()) text += "  " + r.note + "\n";
            ++found;
        }
    }
    if (!found) text += "Nema rezultata.\n";
    MessageBoxW(g_hMain, s2ws(text).c_str(), L"Pretraga", MB_OK | MB_ICONINFORMATION);
}

static void NotifyTodayReminders() {
    if (!g_notifyOnStart) return;
    Date today = Today();
    auto todayRems = RemindersForDate(today);
    auto todayHol = HolidaysOnDate(today);
    if (todayRems.empty() && todayHol.empty()) return;
    string text = "Danasnji pregled: " + DateToISO(today) + "\n\n";
    if (!todayHol.empty()) {
        text += "PRAZNICI\n";
        for (auto& h : todayHol) text += "- " + h + "\n";
        text += "\n";
    }
    if (!todayRems.empty()) {
        text += "PODSETNICI\n";
        for (auto* r : todayRems) text += "- " + r->title + (r->done ? " [zavrseno]" : "") + "\n";
    }
    MessageBoxW(g_hMain, s2ws(text).c_str(), L"Danasnji pregled", MB_OK | MB_ICONINFORMATION);
}

static void CreateAppMenu(HWND hwnd) {
    HMENU menu = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, IDC_EXPORTDAYBTN, L"Izvezi dnevni izvestaj");
    AppendMenuW(fileMenu, MF_STRING, IDC_EXPORTYEARBTN, L"Izvezi godisnji izvestaj");
    AppendMenuW(fileMenu, MF_STRING, IDC_BACKUPBTN, L"Backup podataka");
    AppendMenuW(fileMenu, MF_STRING, IDC_RESTOREBTN, L"Restore podataka");
    AppendMenuW(fileMenu, MF_STRING, 2001, L"Izvezi CSV predstojećih obaveza");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, 2002, L"Izlaz");

    HMENU viewMenu = CreatePopupMenu();
    AppendMenuW(viewMenu, MF_STRING, IDC_UPCOMINGBTN, L"Predstojece obaveze");
    AppendMenuW(viewMenu, MF_STRING, IDC_YEARBTN, L"Godisnji pregled praznika");
    AppendMenuW(viewMenu, MF_STRING, 2003, L"Oznaci/vrati zavrseno");

    HMENU settingsMenu = CreatePopupMenu();
    AppendMenuW(settingsMenu, MF_STRING, IDC_NOTIFYSTART, L"Ukljuci/iskljuci start obavestenje");

    AppendMenuW(menu, MF_POPUP, (UINT_PTR)fileMenu, L"Fajl");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)viewMenu, L"Pregled");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)settingsMenu, L"Podesavanja");
    SetMenu(hwnd, menu);
}

static LRESULT OnCreate(HWND hwnd) {
    InitCommonControls();
    CreateAppMenu(hwnd);

    NONCLIENTMETRICSW ncm{ sizeof(ncm) };
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    HFONT font = CreateFontIndirectW(&ncm.lfMessageFont);

    g_hCal = CreateWindowExW(0, MONTHCAL_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        18, 18, 320, 220, hwnd, (HMENU)IDC_MONTHCAL, g_hInst, nullptr);

    g_hHolidayList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        18, 248, 500, 150, hwnd, (HMENU)IDC_HOLIDAYLIST, g_hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_hHolidayList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    AddColumns(g_hHolidayList, {L"Praznik / Napomena"}, {470});

    g_hReminderList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        18, 412, 820, 220, hwnd, (HMENU)IDC_REMINDERLIST, g_hInst, nullptr);
    ListView_SetExtendedListViewStyle(g_hReminderList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    AddColumns(g_hReminderList, {L"Naslov", L"Ponavljanje", L"Status", L"Napomena"}, {210, 110, 90, 390});

    CreateWindowW(L"STATIC", L"Naslov", WS_CHILD | WS_VISIBLE, 360, 20, 80, 20, hwnd, nullptr, g_hInst, nullptr);
    g_hTitleEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        360, 42, 290, 28, hwnd, (HMENU)IDC_TITLEEDIT, g_hInst, nullptr);

    CreateWindowW(L"STATIC", L"Napomena", WS_CHILD | WS_VISIBLE, 360, 80, 100, 20, hwnd, nullptr, g_hInst, nullptr);
    g_hNoteEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        360, 104, 290, 110, hwnd, (HMENU)IDC_NOTEEDIT, g_hInst, nullptr);

    CreateWindowW(L"STATIC", L"Ponavljanje", WS_CHILD | WS_VISIBLE, 670, 20, 100, 20, hwnd, nullptr, g_hInst, nullptr);
    g_hRecurrence = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        670, 42, 170, 200, hwnd, (HMENU)IDC_RECURCOMBO, g_hInst, nullptr);
    SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Jednom");
    SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Dnevno");
    SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Nedeljno");
    SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Mesecno");
    SendMessageW(g_hRecurrence, CB_ADDSTRING, 0, (LPARAM)L"Godisnje");
    SendMessageW(g_hRecurrence, CB_SETCURSEL, 0, 0);

    CreateWindowW(L"BUTTON", L"Dodaj", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 84, 170, 34, hwnd, (HMENU)IDC_ADDREM, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Izmeni", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 124, 170, 34, hwnd, (HMENU)IDC_UPDATEREM, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Obrisi", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 164, 170, 34, hwnd, (HMENU)IDC_DELETEREM, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Ocisti polja", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 204, 170, 34, hwnd, (HMENU)IDC_CLEARBTN, g_hInst, nullptr);

    CreateWindowW(L"STATIC", L"Pretraga", WS_CHILD | WS_VISIBLE, 360, 248, 100, 20, hwnd, nullptr, g_hInst, nullptr);
    g_hSearchEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        360, 272, 210, 28, hwnd, (HMENU)IDC_SEARCHEDIT, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Trazi", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        580, 270, 80, 32, hwnd, (HMENU)IDC_SEARCHBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Predstojece", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 270, 170, 32, hwnd, (HMENU)IDC_UPCOMINGBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Godisnji pregled", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 310, 170, 32, hwnd, (HMENU)IDC_YEARBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Izvoz dana", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 312, 120, 32, hwnd, (HMENU)IDC_EXPORTDAYBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Izvoz godine", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        490, 312, 120, 32, hwnd, (HMENU)IDC_EXPORTYEARBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Backup", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 352, 120, 32, hwnd, (HMENU)IDC_BACKUPBTN, g_hInst, nullptr);
    CreateWindowW(L"BUTTON", L"Restore", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        490, 352, 120, 32, hwnd, (HMENU)IDC_RESTOREBTN, g_hInst, nullptr);

    g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"Spremno", WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, (HMENU)IDC_STATUSBAR, g_hInst, nullptr);

    for (HWND child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);

    LoadData();
    RefreshAll();
    PostMessageW(hwnd, WM_APP + 1, 0, 0);
    return 0;
}

static void ResizeChildren(HWND hwnd, int width, int height) {
    SendMessageW(g_hStatus, WM_SIZE, 0, 0);
    MoveWindow(g_hReminderList, 18, 412, width - 36, height - 480, TRUE);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: return OnCreate(hwnd);
        case WM_SIZE: ResizeChildren(hwnd, LOWORD(lParam), HIWORD(lParam)); return 0;
        case WM_APP + 1: NotifyTodayReminders(); return 0;
        case WM_NOTIFY: {
            LPNMHDR hdr = (LPNMHDR)lParam;
            if (hdr->idFrom == IDC_MONTHCAL && hdr->code == MCN_SELECT) {
                RefreshAll();
            } else if (hdr->idFrom == IDC_REMINDERLIST && hdr->code == LVN_ITEMCHANGED) {
                NMLISTVIEW* lv = (NMLISTVIEW*)lParam;
                if ((lv->uNewState & LVIS_SELECTED) && lv->iItem >= 0) {
                    LVITEMW item{};
                    item.mask = LVIF_PARAM;
                    item.iItem = lv->iItem;
                    ListView_GetItem(g_hReminderList, &item);
                    LoadReminderIntoEditors((int)item.lParam);
                }
            } else if (hdr->idFrom == IDC_REMINDERLIST && hdr->code == NM_DBLCLK) {
                ToggleDoneSelected();
            }
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_ADDREM: AddReminderFromUI(); break;
                case IDC_UPDATEREM: UpdateReminderFromUI(); break;
                case IDC_DELETEREM: DeleteSelectedReminder(); break;
                case IDC_CLEARBTN: ClearEditors(); SetStatus(L"Polja ociscena."); break;
                case IDC_SEARCHBTN: SearchReminders(); break;
                case IDC_UPCOMINGBTN: ShowUpcomingWindow(hwnd); break;
                case IDC_YEARBTN: ShowYearWindow(hwnd); break;
                case IDC_EXPORTDAYBTN: ExportDailyReport(); break;
                case IDC_EXPORTYEARBTN: ExportYearReport(); break;
                case IDC_BACKUPBTN: BackupData(); break;
                case IDC_RESTOREBTN: RestoreData(); break;
                case IDC_NOTIFYSTART:
                    g_notifyOnStart = !g_notifyOnStart;
                    SaveData();
                    MessageBoxW(hwnd, g_notifyOnStart ? L"Start obavestenje je ukljuceno." : L"Start obavestenje je iskljuceno.", APP_TITLE, MB_OK | MB_ICONINFORMATION);
                    break;
                case 2001: ExportCSVUpcoming(); break;
                case 2002: DestroyWindow(hwnd); break;
                case 2003: ToggleDoneSelected(); break;
            }
            return 0;
        }
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_DESTROY: SaveData(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    g_hInst = hInstance;
    INITCOMMONCONTROLSEX icex{ sizeof(icex), ICC_DATE_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = APP_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hbrBackground = CreateSolidBrush(RGB(24, 28, 34));
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, APP_CLASS, APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 720,
        nullptr, nullptr, hInstance, nullptr);

    g_hMain = hwnd;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
