#include <X11/keysym.h>
#include <string>

#include <atspi/atspi.h>

typedef struct {
  AtspiEventListener *pFocusListener;
  AtspiEventListener *pCaretListener;
  AtspiText *pAccessibleText;
} EditorExternal;

EditorExternal INSTANCE;

typedef enum {
  EDIT_CHAR, EDIT_WORD, EDIT_SENTENCE, EDIT_PARAGRAPH, EDIT_FILE, EDIT_LINE, EDIT_PAGE, EDIT_SELECTION,
} EditDistance;

void dasher_editor_external_output(const char *szText/*, int iOffset unused*/) {
  atspi_generate_keyboard_event(0, szText, ATSPI_KEY_STRING, NULL);
}

void dasher_editor_external_delete(/*int iLength, int iOffset*/) {
  atspi_generate_keyboard_event(XK_BackSpace, NULL, ATSPI_KEY_SYM, NULL);
}

std::string dasher_editor_external_get_context(int iOffset, int iLength) {
  AtspiText *textobj = INSTANCE.pAccessibleText;
  if (textobj != nullptr) {
    char* text = atspi_text_get_text(textobj, iOffset, iOffset + iLength, NULL);
    if (text != nullptr) {
      std::string context = text;
      g_free(text);
      return context;
    }
  }
  return "";
}

int dasher_editor_external_get_offset() {
  AtspiText *textobj = INSTANCE.pAccessibleText;
  if (!textobj) return 0;
  if (atspi_text_get_n_selections(textobj, NULL) == 0)
    return atspi_text_get_caret_offset(textobj, NULL);
  AtspiRange *range = atspi_text_get_selection(textobj, 0, NULL);
  int ret = std::min(range->start_offset, range->end_offset);
  g_free(range);
  return ret;
}

void dasher_editor_external_handle_focus(const AtspiEvent *pEvent) {
  AtspiText *textobj = INSTANCE.pAccessibleText;
  if (textobj) {
    g_object_unref(textobj);
    textobj = NULL;
  }
  AtspiAccessible *acc = pEvent->source;
  g_object_ref(acc);
  textobj = atspi_accessible_get_text_iface(acc);
  INSTANCE.pAccessibleText = textobj;
  if (textobj) {
    g_object_ref(textobj);
  }
  g_object_unref(acc);
}

void dasher_editor_external_handle_caret(const AtspiEvent *pEvent) {
  AtspiText *textobj = INSTANCE.pAccessibleText;
  if (textobj) {
    g_object_unref(textobj);
    textobj = NULL;
  }
  AtspiAccessible *acc = pEvent->source;
  g_object_ref(acc);
  textobj = atspi_accessible_get_text(acc);
  INSTANCE.pAccessibleText = textobj;
  g_object_unref(acc);
}

void dasher_editor_external_move(bool bForwards, EditDistance iDist) {
  AtspiText *textobj = INSTANCE.pAccessibleText;
  if (textobj == nullptr) return;
  GError *err = nullptr;
  int caret_pos = atspi_text_get_caret_offset(textobj, &err);
  if (err != nullptr) {
    fprintf(stderr, "Failed to get the caret: %s\n", err->message);
    g_error_free(err);
    return;
  }
  int length = atspi_text_get_character_count(textobj, &err);
  if (err != nullptr) {
    fprintf(stderr, "Failed to get the character count: %s\n", err->message);
    g_error_free(err);
    return;
  }
  if (length == 0) return;
  long new_position = caret_pos;
  AtspiTextBoundaryType boundary = ATSPI_TEXT_BOUNDARY_CHAR;
  switch (iDist) {
    case EDIT_CHAR:
      if (bForwards) new_position = std::min<long>(caret_pos + 1, length - 1);
      else new_position = std::max<long>(caret_pos - 1, 0);
      break;
    case EDIT_FILE:
      if (bForwards) new_position = length - 1;
      else new_position = 0;
      break;
    case EDIT_WORD:
      boundary = ATSPI_TEXT_BOUNDARY_WORD_START;
      break;
    case EDIT_LINE:
      boundary = ATSPI_TEXT_BOUNDARY_LINE_START;
      break;
    case EDIT_SENTENCE:
      boundary = ATSPI_TEXT_BOUNDARY_SENTENCE_START;
      break;
    default:
      break;
  }
  AtspiTextRange* range = nullptr;
  if (boundary != ATSPI_TEXT_BOUNDARY_CHAR) {
    if (bForwards) range = atspi_text_get_text_after_offset(textobj, caret_pos, boundary, &err);
    else range = atspi_text_get_text_before_offset(textobj, caret_pos, boundary, &err);
    if (err != nullptr) {
      fprintf(stderr, "Failed to get the text after/befor the offset: %s\n", err->message);
      g_error_free(err);
      return;
    }
    if (range != nullptr) new_position = range->start_offset;
    g_free(range);
  }
  atspi_text_set_caret_offset(textobj, new_position, &err);
  if (err != nullptr) {
    fprintf(stderr, "Failed to set the caret offset: %s\n", err->message);
    g_error_free(err);
    return;
  }
}

std::string dasher_editor_external_get_text_around_cursor(EditDistance distance) {
  AtspiText *textobj = INSTANCE.pAccessibleText;
  if (textobj == nullptr) return "";
  GError *err = nullptr;
  int caret_pos = atspi_text_get_caret_offset(textobj, &err);
  if (err != nullptr) {
    fprintf(stderr, "Failed to get the caret offset: %s\n", err->message);
    g_error_free(err);
    return "";
  }
  std::string text;
  AtspiTextGranularity granularity;
  switch (distance) {
    case EDIT_FILE: {
      int length = atspi_text_get_character_count(textobj, nullptr);
      char* gtext = atspi_text_get_text(textobj, 0, length, nullptr);
      text = gtext;
      g_free(gtext);
      return text;
    }
      break;
    case EDIT_WORD:
      granularity = ATSPI_TEXT_GRANULARITY_WORD;
      break;
    case EDIT_LINE:
      granularity = ATSPI_TEXT_GRANULARITY_LINE;
      break;
    case EDIT_SENTENCE:
      granularity = ATSPI_TEXT_GRANULARITY_SENTENCE;
      break;
    case EDIT_PARAGRAPH:
      // TODO: figure out why ATSPI_TEXT_GRANULARITY_PARAGRAPH doesn't work.
      granularity = ATSPI_TEXT_GRANULARITY_PARAGRAPH;
      break;
    default:
      return "";
  }
  AtspiTextRange* range = atspi_text_get_string_at_offset(textobj, caret_pos, granularity, &err);
  if (err != nullptr) {
    fprintf(stderr, "Failed to get the caret offset: %s\n", err->message);
    g_error_free(err);
    return "";
  }
  if (range != nullptr) {
    text = range->content;
    g_free(range);
  }
  return text;
}

static void focus_listener(AtspiEvent *pEvent, void *pUserData) {
  fprintf(stderr, "Focus\n");
  dasher_editor_external_handle_focus(pEvent);
  dasher_editor_external_move(true, EDIT_CHAR);
}

static void caret_listener(AtspiEvent *pEvent, void *pUserData) {
  fprintf(stderr, "Caret\n");
  dasher_editor_external_handle_caret(pEvent);
  fprintf(stderr, "%s\n", dasher_editor_external_get_text_around_cursor(EDIT_WORD).c_str());
}

void init() {
  if (atspi_init() > 1) {
    g_message("Could not initialise SPI - accessibility options disabled");
    return;
  }
  INSTANCE.pFocusListener = atspi_event_listener_new(focus_listener, NULL, NULL);
  INSTANCE.pCaretListener = atspi_event_listener_new(caret_listener, NULL, NULL);
  INSTANCE.pAccessibleText = NULL;
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
  dasher_editor_external_output("Test");
  atspi_event_main();
  return 0;
}
