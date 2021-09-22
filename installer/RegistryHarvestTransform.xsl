<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:wix="http://schemas.microsoft.com/wix/2006/wi"
    xmlns="http://schemas.microsoft.com/wix/2006/wi"
    version="1.0"
    exclude-result-prefixes="xsl wix">

  <xsl:template name="reg-harvest-transform-replace-string">
    <xsl:param name="source" />
    <xsl:param name="find" />
    <xsl:param name="repl" />
    <xsl:choose>
      <xsl:when test="$source = '' or $find = '' or not($find)" >
        <xsl:value-of select="$source" />
      </xsl:when>
      <xsl:when test="contains($source, $find)">
        <xsl:value-of select="substring-before($source, $find)" />
        <xsl:value-of select="$repl" />
        <xsl:call-template name="reg-harvest-transform-replace-string">
          <xsl:with-param name="source" select="substring-after($source, $find)" />
          <xsl:with-param name="find" select="$find" />
          <xsl:with-param name="repl" select="$repl" />
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$source" />
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="reg-harvest-transform-hkey">

    <xsl:param name="hkey" />
    <xsl:param name="file-id" />
    <xsl:param name="generated-file-id" />
    <xsl:param name="keypath-counter" />

    <xsl:choose>

      <!-- remove registry nodes whose root is not the same as hkey -->
      <xsl:when test="@Root and @Root != $hkey" />

      <!-- replace generated file id with specified file id -->
      <xsl:when test="parent::*/@*[contains(., concat('[!', $generated-file-id, ']'))]">
        <xsl:attribute name="{name(.)}">
          <xsl:call-template name="reg-harvest-transform-replace-string">
            <xsl:with-param name="source" select="." />
            <xsl:with-param name="find" select="concat('[!', $generated-file-id, ']')" />
            <xsl:with-param name="repl" select="concat('[!', $file-id, ']')" />
          </xsl:call-template>
        </xsl:attribute>
      </xsl:when>

      <!-- no other changes -->
      <xsl:otherwise>
        <xsl:copy>
          <xsl:for-each select="@*|node()">
            <xsl:call-template name="reg-harvest-transform-hkey">
              <xsl:with-param name="hkey" select="$hkey" />
              <xsl:with-param name="file-id" select="$file-id" />
              <xsl:with-param name="generated-file-id" select="$generated-file-id" />
              <xsl:with-param name="keypath-counter" select="-1" />
            </xsl:call-template>
          </xsl:for-each>
          <!-- apply keypath attribute to node with matching index -->
          <xsl:if test="@Root and $keypath-counter = 1">
            <xsl:attribute name="KeyPath">
              <xsl:text>yes</xsl:text>
            </xsl:attribute>
          </xsl:if>
        </xsl:copy>
      </xsl:otherwise>

    </xsl:choose>

  </xsl:template>

  <xsl:template name="reg-harvest-transform">

    <xsl:param name="file-id" />
    <xsl:param name="hkcu-component-id" />
    <xsl:param name="hkcu-component-guid" />
    <xsl:param name="hkcu-keypath-position" />
    <xsl:param name="hklm-component-id" />
    <xsl:param name="hklm-component-guid" />
    <xsl:param name="hklm-keypath-position" />

    <!-- find generated file id -->
    <xsl:variable name="generated-file-id">
      <xsl:value-of select="//wix:Component/wix:File/@Id"/>
    </xsl:variable>
    
    <xsl:choose>

      <!-- replace generated component id with specified component id and strip generated file element-->
      <xsl:when test="self::wix:Component">
        <xsl:copy>
          <!-- replace id and guid -->
          <xsl:attribute name="Id">
            <xsl:value-of select="$hkcu-component-id"/>
          </xsl:attribute>
          <xsl:attribute name="Guid">
            <xsl:value-of select="$hkcu-component-guid"/>
          </xsl:attribute>
          <!-- create component for HKCU entries -->
          <xsl:for-each select="@*[name()!='Id' and name()!='Guid']|node()[name()!='File']">
            <xsl:variable name="pos">
              <xsl:value-of select="position()"/>
            </xsl:variable>
            <xsl:call-template name="reg-harvest-transform-hkey">
              <xsl:with-param name="hkey" select="'HKCU'" />
              <xsl:with-param name="file-id" select="$file-id" />
              <xsl:with-param name="generated-file-id" select="$generated-file-id" />
              <xsl:with-param name="keypath-counter" select="$hkcu-keypath-position - count(preceding-sibling::*[@Root = 'HKCU' and position() &lt; $pos])" />
            </xsl:call-template>
          </xsl:for-each>
        </xsl:copy>
        <xsl:copy>
          <!-- replace id and guid -->
          <xsl:attribute name="Id">
            <xsl:value-of select="$hklm-component-id"/>
          </xsl:attribute>
          <xsl:attribute name="Guid">
            <xsl:value-of select="$hklm-component-guid"/>
          </xsl:attribute>
          <!-- create component for HKLM entries -->
          <xsl:for-each select="@*[name()!='Id' and name()!='Guid']|node()[name()!='File']">
            <xsl:variable name="pos">
              <xsl:value-of select="position()"/>
            </xsl:variable>
            <xsl:call-template name="reg-harvest-transform-hkey">
              <xsl:with-param name="hkey" select="'HKLM'" />
              <xsl:with-param name="file-id" select="$file-id" />
              <xsl:with-param name="generated-file-id" select="$generated-file-id" />
              <xsl:with-param name="keypath-counter" select="$hklm-keypath-position - count(preceding-sibling::*[@Root = 'HKLM' and position() &lt; $pos])" />
            </xsl:call-template>
          </xsl:for-each>
        </xsl:copy>
      </xsl:when>

      <!-- no other changes -->
      <xsl:otherwise>
        <xsl:copy>
          <xsl:apply-templates select="@*|node()"/>
        </xsl:copy>
      </xsl:otherwise>

    </xsl:choose>

  </xsl:template>

</xsl:stylesheet>