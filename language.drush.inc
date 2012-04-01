<?php

/**
 * Implementation of hook_drush_command().
 *
 * @See drush_parse_command() for a list of recognized keys.
 *
 * @return
 *   An associative array describing your command(s).
 */
function language_drush_command() {
  $items = array();
  $items['language-add'] = array(
    'description' => "Add and import a new language definition",
    'aliases' => array('langadd'),
  );
  $items['language-enable'] = array(
    'description' => "Enable an already defined language",
    'aliases' => array('langen'),
  );
  $items['language-disable'] = array(
    'description' => "Disable an already defined language",
    'aliases' => array('langdis'),
  );
  $items['language-default'] = array(
    'description' => "Assign an enabled language as default",
    'aliases' => array('langdef'),
  );
  return $items;
}

/**
 * Add a language
 */
function drush_language_add() {
  if (!function_exists('locale_add_language')) {
    drush_set_error(dt('Could not add language. Is the \'locale\' module enabled?'));
    return;
  }

  $args = func_get_args();
  if (count($args) == 0) {
    drush_set_error(dt('Please provide one or more language codes as arguments.'));
    return;
  }

  foreach ($args as $langcode) {
    if (array_key_exists($langcode, language_list())) {
      drush_log(dt('The language with code !code already exists.', array('!code' => $langcode)), 'warning');
    }
    else {
      // Predefined language selection.
      include_once DRUPAL_ROOT . '/includes/iso.inc';
      $predefined = _locale_get_predefined_list();
      if (!isset($predefined[$langcode])) {
        drush_log(dt('Invalid language code !language', array('!language' => $langcode)), 'warning');
      }
      else {
        // Add the language definition
        locale_add_language($langcode);
        // See if we have language files to import for the newly added
        // language, collect and import them.
        if ($batch = locale_batch_by_language($langcode, '_locale_batch_language_finished')) {
          batch_set($batch);

          $batch =& batch_get();
          $batch['progressive'] = FALSE;

          // Process the batch.
          drush_backend_batch_process();
        }
        drush_log(dt('Added language: !language', array('!language' => $langcode)), 'ok');
      }
    }
  }
}

/**
 * Enable a language
 */
function drush_language_enable() {
  $args = func_get_args();
  if (count($args) == 0) {
    drush_set_error(dt('Please provide one or more language codes as arguments.'));
    return;
  }

  foreach ($args as $langcode) {
    $languages = language_list();
    if (array_key_exists($langcode, $languages)) {
      if (!$languages[$langcode]->enabled) {
        // disable the default english
        db_update('languages')
          ->condition('language', $langcode)
          ->fields(array(
            'enabled' => 1,
          ))
          ->execute();

        // Changing the language settings impacts the interface.
        cache_clear_all('*', 'cache_page', TRUE);
        drush_log(dt("Enabled language : !language ", array('!language' => $langcode)), 'ok');
      }
      else {
        drush_log(dt("Language already enabled: !language ", array('!language' => $langcode)), 'warning');
      }
    }
    else {
      drush_log(dt("Specified language does not exist !language", array('!language' => $langcode)), 'warning');
    }
  }
}

/**
 * Disables a language
 */
function drush_language_disable() {
  $args = func_get_args();
  if (count($args) == 0) {
    drush_set_error(dt('Please provide one or more language codes as arguments.'));
    return;
  }

  foreach ($args as $langcode) {
    $languages = language_list();
    if (array_key_exists($langcode, $languages)) {
      if ($languages[$langcode]->enabled) {
        // disable the default english
        db_update('languages')
          ->condition('language', $langcode)
          ->fields(array(
            'enabled' => 0,
          ))
          ->execute();

        // Changing the language settings impacts the interface.
        cache_clear_all('*', 'cache_page', TRUE);
        drush_log(dt("Disabled language : !language ", array('!language' => $langcode)), 'ok');
      }
      else {
        drush_print(dt("Language already disabled: !language ", array('!language' => $langcode)), 'warning');
      }
    }
  }
}

/**
 * Assigns the default language
 */
function drush_language_default() {
  $args = func_get_args();
  if (count($args) == 0) {
    drush_set_error(dt('Please provide one or more language codes as arguments.'));
    return;
  }

  foreach ($args as $langcode) {
    // get all the languages
    $languages = language_list();
    if (array_key_exists($langcode, $languages)) {
      variable_set('language_default', (object) $languages[$langcode]);
      drush_log(dt("!language assigned as default", array('!language' => $langcode)), 'ok');
    }
    else {
      drush_log(dt("Specified language does not exist !language", array('!language' => $langcode)), 'warning');
    }
  }
}
