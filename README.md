# Drush 9 language commands

## Upgrading

At the time of this commit (Drush 9.0.0-beta5), Drush 9 needs commands using
Drupal services to be placed in a module, which was not necessary for Drush 8.

Quoting Greg Anderson in https://github.com/drush-ops/drush/issues/3050 :  

<blockquote>Drush extensions that are not part of a module can be policy files 
  (hooks) or standalone commands, but cannot do dependency injection.
  </blockquote>
  
As a result this plugin is no longer of type `drupal-drush` but is a normal
Drupal module. Assuming it is being used in a Composer `drupal-project` workflow,
this means:

* Removing the `drush/drush_language/` or `drush/contrib/drush_language` 
  directory.
* Running `composer update` to obtain the new version of the plugin implemented
  as a module and placed in the `web/modules/contrib/drush_language` directory.
* Enabling the `drush_language` module to make the commands discoverable by
  Drush.

