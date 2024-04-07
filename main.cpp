#include <X11/keysym.h>
#include <string>

#include <atspi/atspi.h>

typedef enum {
  EDIT_CHAR,
  EDIT_WORD,
  EDIT_SENTENCE,
  EDIT_PARAGRAPH,
  EDIT_FILE,
  EDIT_LINE,
  EDIT_PAGE,
  EDIT_SELECTION
} EditDistance;

typedef struct {
  AtspiEventListener* pFocusListener;
  AtspiEventListener* pCaretListener;
  AtspiText* pAccessibleText;
} EditorExternal;

EditorExternal INSTANCE;

void output(const char* szText) {
  atspi_generate_keyboard_event(0, szText, ATSPI_KEY_STRING, NULL);
}

void deleteChar() {
  atspi_generate_keyboard_event(XK_BackSpace, NULL, ATSPI_KEY_SYM, NULL);
}

std::string get_context(int iOffset, int iLength) {
  AtspiText* textobj = INSTANCE.pAccessibleText;
  if (textobj!=NULL) {
    char* text = atspi_text_get_text(textobj, iOffset, iOffset+iLength, NULL);
    if (text!=NULL) {
      std::string context = text;
      free(text);
      return context;
    }
  }
  return "";
}

void move(bool bForwards, EditDistance iDist) {
  AtspiText* textobj = INSTANCE.pAccessibleText;
  if (textobj==NULL) return;
  GError* err = NULL;
  int caret_pos = atspi_text_get_caret_offset(textobj, &err);
  if (err!=NULL) {
    fprintf(stderr, "Failed to get the caret: %s\n", err->message);
    g_error_free(err);
    return;
  }
  int length = atspi_text_get_character_count(textobj, &err);
  if (err!=NULL) {
    fprintf(stderr, "Failed to get the character count: %s\n", err->message);
    g_error_free(err);
    return;
  }
  if (length==0) return;
  long new_position = caret_pos;
  AtspiTextBoundaryType boundary = ATSPI_TEXT_BOUNDARY_CHAR;
  switch (iDist) {
    case EDIT_CHAR:
      if (bForwards) new_position=std::min<long>(caret_pos+1, length-1);
      else new_position=std::max<long>(caret_pos-1, 0);
      break;
    case EDIT_FILE:
      if (bForwards) new_position=length-1;
      else new_position=0;
      break;
    case EDIT_WORD:
      boundary=ATSPI_TEXT_BOUNDARY_WORD_START;
      break;
    case EDIT_LINE:
      boundary=ATSPI_TEXT_BOUNDARY_LINE_START;
      break;
    case EDIT_SENTENCE:
      boundary=ATSPI_TEXT_BOUNDARY_SENTENCE_START;
      break;
    default:
      break;
  }
  AtspiTextRange* range = NULL;
  if (boundary!=ATSPI_TEXT_BOUNDARY_CHAR) {
    if (bForwards) range=atspi_text_get_text_after_offset(textobj, caret_pos, boundary, &err);
    else range=atspi_text_get_text_before_offset(textobj, caret_pos, boundary, &err);
    if (err!=NULL) {
      fprintf(stderr, "Failed to get the text after/befor the offset: %s\n", err->message);
      g_error_free(err);
      return;
    }
    if (range!=NULL) new_position=range->start_offset;
    g_free(range);
  }
  atspi_text_set_caret_offset(textobj, new_position, &err);
  if (err!=NULL) {
    fprintf(stderr, "Failed to set the caret offset: %s\n", err->message);
    g_error_free(err);
    return;
  }
}

std::string get_text_around_cursor(EditDistance distance) {
  AtspiText* textobj = INSTANCE.pAccessibleText;
  if (textobj==NULL) return "";
  GError* err = NULL;
  int caret_pos = atspi_text_get_caret_offset(textobj, &err);
  if (err!=NULL) {
    fprintf(stderr, "Failed to get the caret offset: %s\n", err->message);
    g_error_free(err);
    return "";
  }
  std::string text;
  AtspiTextGranularity granularity;
  switch (distance) {
    case EDIT_FILE: {
      int length = atspi_text_get_character_count(textobj, NULL);
      char* textBuffer = atspi_text_get_text(textobj, 0, length, NULL);
      text=textBuffer;
      free(textBuffer);
      return text;
    }
      break;
    case EDIT_WORD:
      granularity=ATSPI_TEXT_GRANULARITY_WORD;
      break;
    case EDIT_LINE:
      granularity=ATSPI_TEXT_GRANULARITY_LINE;
      break;
    case EDIT_SENTENCE:
      granularity=ATSPI_TEXT_GRANULARITY_SENTENCE;
      break;
    case EDIT_PARAGRAPH:
      //TODO: figure out why ATSPI_TEXT_GRANULARITY_PARAGRAPH doesn't work.
      granularity=ATSPI_TEXT_GRANULARITY_PARAGRAPH;
      break;
    default:
      return "";
  }
  AtspiTextRange* range = atspi_text_get_string_at_offset(textobj, caret_pos, granularity, &err);
  if (err!=NULL) {
    fprintf(stderr, "Failed to get the caret offset: %s\n", err->message);
    g_error_free(err);
    return "";
  }
  if (range!=NULL) {
    text=range->content;
    free(range);
  }
  return text;
}

void handle_event(const AtspiEvent* pEvent) {
  AtspiText* textobj = INSTANCE.pAccessibleText;
  if (textobj) {
    g_object_unref(textobj);
    textobj=NULL;
  }
  AtspiAccessible* acc = pEvent->source;
  g_object_ref(acc);
  textobj=atspi_accessible_get_text_iface(acc);
  INSTANCE.pAccessibleText=textobj;
  g_object_unref(acc);
}

static void focus_listener(AtspiEvent* pEvent, void* pUserData) {
  fprintf(stderr, "Focus\n");
  handle_event(pEvent);
  if (INSTANCE.pAccessibleText!=NULL) g_object_ref(INSTANCE.pAccessibleText);
  move(true, EDIT_CHAR);
}

static void caret_listener(AtspiEvent* pEvent, void* pUserData) {
  fprintf(stderr, "Caret\n");
  handle_event(pEvent);
  fprintf(stderr, "%s\n", get_text_around_cursor(EDIT_WORD).c_str());
}

void init() {
  if (atspi_init()>1) {
    g_message("Could not initialise SPI - accessibility options disabled");
    return;
  }
  INSTANCE.pFocusListener=atspi_event_listener_new(focus_listener, NULL, NULL);
  INSTANCE.pCaretListener=atspi_event_listener_new(caret_listener, NULL, NULL);
  INSTANCE.pAccessibleText=NULL;
  atspi_event_listener_register(INSTANCE.pFocusListener, "object:state-changed:focused", NULL);
  atspi_event_listener_register(INSTANCE.pCaretListener, "object:text-caret-moved", NULL);
}

void unregister() {
  atspi_event_listener_deregister(INSTANCE.pFocusListener, "object:state-changed:focused", NULL);
  atspi_event_listener_deregister(INSTANCE.pCaretListener, "object:text-caret-moved", NULL);
  g_object_unref(INSTANCE.pFocusListener);
  g_object_unref(INSTANCE.pCaretListener);
}

int main() {
  init();
  output("Test");
  atspi_event_main();
  return 0;
}
