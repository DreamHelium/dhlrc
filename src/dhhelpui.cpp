#include "dhhelpui.h"
#include "mainwindow.h"

#include <libintl.h>
#define _(str) gettext (str)
/* Marks a string in the topic table for extraction; it is translated through
 * `_ ()` when the row is built. */
#define N_(str) (str)

#include <QAbstractItemView>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QLocale>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>

namespace
{
/* The one window, so a second request only raises it. It is a plain top-level
 * window (no parent), which is what makes it a window of its own rather than a
 * widget inside the main window. */
QPointer<DhHelpUI> helpWindow;

/* One article the help window can show: the id callers select it by, its
 * title (a message id) and the resource that holds it. */
struct Topic
{
  const char *id;
  const char *title;
  const char *resource;
};

constexpr Topic topics[] = {
  { "usage", N_ ("Quick Start"), "usage" },
  { "region-create", N_ ("Create Region"), "region_create" },
};

/* Where the row stores the topic it stands for. */
constexpr int topicRole = Qt::UserRole;

/* The locale the message catalogue is being read for, e.g. `zh_CN`, without
 * the codeset or the modifier; an empty string when the messages are not
 * translated at all.
 *
 * This has to agree with what gettext really does, or the article would be in
 * a different language from the window around it:
 *
 * - `LANGUAGE` comes first, but it is *ignored* while the locale named by
 *   `LC_ALL`/`LC_MESSAGES`/`LANG` is `C` or `POSIX`, where no catalogue is
 *   read at all. So `LANG=C` means English even when `LANGUAGE=zh_CN` is
 *   exported, which is how a user forces English.
 * - Otherwise that locale is used. A Windows session sets none of these
 *   variables, so Qt's own answer stands in there. */
QString
catalogLocale ()
{
  const auto fromEnvironment = [] (const char *name)
    { return QString::fromLocal8Bit (qgetenv (name)).trimmed (); };
  /* One entry of a `a:b` list, which may also carry a codeset and a modifier:
   * `zh_CN.UTF-8` and `sr_RS@latin` both name a locale inside something
   * longer. */
  const auto normalise = [] (const QString &name)
    {
      return name.section (':', 0, 0).section ('.', 0, 0).section ('@', 0, 0);
    };
  const auto usable = [] (const QString &name)
    {
      return !name.isEmpty () && name != QLatin1String ("C")
             && name != QLatin1String ("POSIX");
    };

  QString locale = fromEnvironment ("LC_ALL");
  if (locale.isEmpty ())
    locale = fromEnvironment ("LC_MESSAGES");
  if (locale.isEmpty ())
    locale = fromEnvironment ("LANG");
  locale = normalise (locale);

  if (usable (locale))
    {
      const QString language = fromEnvironment ("LANGUAGE");
      if (!language.isEmpty ())
        {
          /* A list, most wanted first; the first one is what a catalogue
           * exists for, in the same way gettext walks it. */
          for (const QString &entry : language.split (':', Qt::SkipEmptyParts))
            if (usable (normalise (entry)))
              return normalise (entry);
        }
      return locale;
    }

  /* Nothing usable was named. On Windows that is the normal case, so Qt's
   * answer is tried there before giving up; on a `C` locale it is not, since
   * `C` really does mean English. */
  if (locale.isEmpty ())
    {
      const QString fromQt = QLocale::system ().name ();
      if (usable (fromQt))
        return fromQt;
    }
  return {};
}

/* The resource holding article `name` for the current language:
 * `name.<locale>.md`, then `name.<language>.md`, then `name.md`. The plain
 * name is returned even when nothing matches, so the caller reports the miss
 * itself. */
QString
articleResource (const QString &name)
{
  const QString base = QStringLiteral (":/cn/dh/dhlrc/") + name;
  QStringList suffixes;
  const QString locale = catalogLocale ();
  if (!locale.isEmpty ())
    {
      suffixes.append (QStringLiteral (".") + locale);
      const QString language = locale.section ('_', 0, 0);
      if (language != locale)
        suffixes.append (QStringLiteral (".") + language);
    }
  suffixes.append (QString ());
  for (const QString &suffix : suffixes)
    {
      const QString path = base + suffix + QStringLiteral (".md");
      if (QFile::exists (path))
        return path;
    }
  return base + QStringLiteral (".md");
}

/* The article's text, or an empty string when it cannot be read. */
QString
readArticle (const QString &resource)
{
  QFile file (resource);
  if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
    return {};
  return QString::fromUtf8 (file.readAll ());
}

/* The topic whose article `link` names, or an empty id. Handles both the
 * `topic:<id>` form and a link to the resource file a topic is built from
 * (`region_create.md`, with or without an anchor). */
QString
topicForLink (const QString &link)
{
  if (link.startsWith (QLatin1String ("topic:")))
    return link.mid (6);
  const QString file = QFileInfo (link.section ('#', 0, 0)).fileName ();
  for (const auto &topic : topics)
    if (file == QLatin1String (topic.resource) + QLatin1String (".md"))
      return QString::fromLatin1 (topic.id);
  return {};
}
}

DhHelpUI::DhHelpUI (QWidget *parent) : QWidget (parent)
{
  MainWindow::ownWindow (this);
  setWindowTitle (_ ("Help"));
  /* So the window is recognisable in the task bar and on the frame; the same
   * theme icon the `Help` menu entry uses. */
  setWindowIcon (QIcon::fromTheme ("help-contents"));
  resize (900, 620);
  setMinimumSize (600, 400);

  auto *layout = new QVBoxLayout (this);
  splitter = new QSplitter (Qt::Horizontal, this);

  auto *listWidget = new QWidget ();
  auto *listLayout = new QVBoxLayout (listWidget);
  listLayout->setContentsMargins (0, 0, 0, 0);
  lineEdit = new QLineEdit ();
  lineEdit->setPlaceholderText (_ ("Search..."));
  lineEdit->setClearButtonEnabled (true);
  listView = new QListView ();
  listView->setSelectionMode (QAbstractItemView::SingleSelection);
  listView->setEditTriggers (QAbstractItemView::NoEditTriggers);
  listLayout->addWidget (lineEdit);
  listLayout->addWidget (listView);

  scrollArea = new QScrollArea ();
  scrollArea->setWidgetResizable (true);
  label = new QLabel ();
  label->setWordWrap (true);
  label->setTextFormat (Qt::MarkdownText);
  label->setAlignment (Qt::AlignTop | Qt::AlignLeft);
  /* Markdown links inside an article can name another article, so they are
   * followed by hand instead of being handed to the browser. */
  label->setOpenExternalLinks (false);
  label->setTextInteractionFlags (Qt::TextBrowserInteraction);
  /* The label sits in a padded page, so the text does not touch the edge. */
  auto *article = new QWidget ();
  auto *articleLayout = new QVBoxLayout (article);
  articleLayout->setContentsMargins (12, 12, 12, 12);
  articleLayout->addWidget (label);
  scrollArea->setWidget (article);

  splitter->addWidget (listWidget);
  splitter->addWidget (scrollArea);
  splitter->setStretchFactor (0, 0);
  splitter->setStretchFactor (1, 1);
  splitter->setCollapsible (1, false);
  splitter->setSizes ({ 260, 640 });
  layout->addWidget (splitter);

  model = new QStandardItemModel (this);
  for (const auto &topic : topics)
    {
      auto *item = new QStandardItem (_ (topic.title));
      item->setData (QString::fromLatin1 (topic.id), topicRole);
      item->setEditable (false);
      model->appendRow (item);
    }
  sortmodel = new QSortFilterProxyModel (this);
  sortmodel->setSourceModel (model);
  listView->setModel (sortmodel);

  /* A fixed string, not a regular expression: the search box takes whatever
   * the user types, and `(` or `[` has to match itself rather than fail to
   * compile. */
  connect (lineEdit, &QLineEdit::textChanged, this,
           [this] (const QString &text)
             { sortmodel->setFilterFixedString (text); });
  connect (listView->selectionModel (), &QItemSelectionModel::currentChanged,
           this,
           [this] (const QModelIndex &current, const QModelIndex &)
             {
               if (current.isValid ())
                 showTopic (current.data (topicRole).toString ());
             });
  connect (label, &QLabel::linkActivated, this, &DhHelpUI::followLink);

  /* Something has to be on screen from the first look. */
  if (model->rowCount () > 0)
    selectTopic (QString::fromLatin1 (topics[0].id));
}

DhHelpUI::~DhHelpUI () = default;

void
DhHelpUI::showHelp (const QString &topic)
{
  if (!helpWindow)
    {
      helpWindow = new DhHelpUI ();
      /* Closing it takes it down; the next call builds a fresh one, which
       * also picks up a language change. */
      helpWindow->setAttribute (Qt::WA_DeleteOnClose);
    }
  helpWindow->show ();
  helpWindow->activateWindow ();
  helpWindow->raise ();
  if (!topic.isEmpty ())
    helpWindow->selectTopic (topic);
}

void
DhHelpUI::free ()
{
  delete helpWindow.data ();
}

void
DhHelpUI::showTopic (const QString &topic)
{
  for (const auto &entry : topics)
    {
      if (topic != QLatin1String (entry.id))
        continue;
      const QString resource
          = articleResource (QString::fromLatin1 (entry.resource));
      const QString text = readArticle (resource);
      if (text.isEmpty ())
        label->setText (QString (_ ("The article could not be read (%1)."))
                            .arg (resource));
      else
        label->setText (text);
      /* A new article starts at its top rather than wherever the last one was
       * scrolled to. */
      scrollArea->verticalScrollBar ()->setValue (0);
      return;
    }
}

void
DhHelpUI::selectTopic (const QString &topic)
{
  for (int row = 0; row < model->rowCount (); row++)
    {
      if (model->item (row)->data (topicRole).toString () != topic)
        continue;
      QModelIndex index = sortmodel->mapFromSource (model->index (row, 0));
      if (!index.isValid ())
        {
          /* The search box is hiding the row, so it is cleared first; this is
           * still synchronous, so the row is there when it is selected. */
          lineEdit->clear ();
          index = sortmodel->mapFromSource (model->index (row, 0));
        }
      if (index.isValid ())
        listView->setCurrentIndex (index);
      return;
    }
}

void
DhHelpUI::followLink (const QString &link)
{
  const QString topic = topicForLink (link);
  if (!topic.isEmpty ())
    {
      selectTopic (topic);
      return;
    }
  const QUrl url (link);
  if (url.isValid ()
      && (url.scheme () == QLatin1String ("http")
          || url.scheme () == QLatin1String ("https")
          || url.scheme () == QLatin1String ("mailto")))
    QDesktopServices::openUrl (url);
}
