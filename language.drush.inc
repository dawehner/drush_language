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
 * Enable a language 
 * 
 */
function drush_language_enable() {
  $args = func_get_args();
  if ($args[0]) {
    $langcode = $args[0];
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
        drush_print(dt("Enabled language : !language ", array('!language' => $langcode)));
      }
      else {
         drush_print(dt("Language already enabled: !language ", array('!language' => $langcode)));
      }
    }
    else {
      drush_print(dt("Specified language does not exist !language", array('!language' => $langcode)));
    }
  }
  else {
    drush_print(dt("Language code required"));
  } 
}
/**
 * Disables a language
 * 
 */
function drush_language_disable() {
  $args = func_get_args();
  if ($args[0]) {
    $langcode = $args[0];
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
        drush_print(dt("Disabled language : !language ", array('!language' => $langcode)));
      }
      else {
         drush_print(dt("Language already disabled: !language ", array('!language' => $langcode)));
      }
    }
  }
  else {
    drush_print(dt("Language code required"));
  } 
}

/**
 * Assigns the default language
 * 
 */
function drush_language_default() {
  $args = func_get_args();
  if ($args[0]) {
    $langcode = $args[0];
    // get all the languages
    $languages = language_list();
    if (array_key_exists($langcode, $languages)) {
      variable_set('language_default', (object) $languages[$langcode]);
      drush_print(dt("!language assigned as default", array('!language' => $langcode)));
    }
    else {
       drush_print(dt("Specified language does not exist !language", array('!language' => $langcode)));
    }
  }
  else {
    drush_print(dt("Language code required"));
  }
}
  